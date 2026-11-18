"""Offline base-LOD decoding, following upstream image.c.

Uses the checkout's existing bitreader/Huffman/RLE/lookup implementations.
Adds channel prediction and paletted deflate paths needed by Dam.
Returns logical top-to-bottom RGBA pixels, before N64 TMEM row swapping.
"""
import struct
import zlib


def native_material_wrap(mode):
    """Keep opaque and face-culling flags alongside the sampler mode."""
    if mode < 0 or mode > 31:
        raise ValueError('Invalid material wrapping flags')
    return native_wrap(mode & 3) | (mode & ~3)


def native_wrap(mode):
    """N64 bit flags MIRROR=1/CLAMP=2 -> native REPEAT=0/CLAMP=1/MIRROR=2.

    Clamp wins when both N64 bits are set, matching the source GL backend.
    """
    if not 0 <= mode <= 3:
        raise ValueError('Invalid N64 texture wrapping mode')
    return 1 if mode & 2 else 2 if mode & 1 else 0


def rgba(fmt, value):
    if fmt == 0:
        return tuple((value >> s) & 255 for s in (24, 16, 8, 0))
    if fmt == 2:
        return ((value >> 16) & 255, (value >> 8) & 255, value & 255, 255)
    if fmt in (1, 3, 9, 10):
        if fmt == 3:
            value = (value << 1) | 1
        return tuple(((value >> s) & 31) * 255 // 31 for s in (11, 6, 1)) + ((value & 1)*255,)
    if fmt in (4, 11, 12):
        return ((value >> 8) & 255,) * 3 + (value & 255,)
    if fmt == 5:
        return ((value >> 4)*17,) * 3 + ((value & 15)*17,)
    if fmt == 6:
        return ((value >> 1)*255//7,) * 3 + ((value & 1)*255,)
    if fmt == 7:
        return (value,) * 3 + (255,)
    if fmt == 8:
        return (value*17,) * 3 + (255,)
    raise ValueError(f"Unsupported pixel format {fmt}")


def unfilter(values, width, method, size):
    if method > 6:
        raise ValueError(f"Unknown texture predictor {method}")
    for i in range(len(values)):
        x, y = i % width, i // width
        left = values[i-1] if x else 0
        above = values[i-width] if y else 0
        diagonal = values[i-width-1] if x and y else 0
        predictors = (left, above, diagonal, left+above-diagonal,
                      int((above-diagonal)/2)+left,
                      int((left-diagonal)/2)+above, (left+above)//2)
        values[i] = (values[i] + predictors[method]) % size


def decode(blob, codec):
    if blob[0] & 64:
        fmt, ncolors = blob[1], blob[2]+1
        if fmt not in (9, 10, 11, 12):
            raise ValueError(f"Unsupported paletted format {fmt}")
        palette = struct.unpack_from(f'>{ncolors}H', blob, 3)
        pos = 3+ncolors*2
        width, height = blob[pos:pos+2]
        pos += 2
        if blob[pos:pos+2] != b'\x11\x72':
            raise ValueError("Missing paletted deflate header")
        data = zlib.decompress(blob[pos+2:], -15)
        stride = width if fmt in (9, 11) else (width+1)//2
        if len(data) != stride*height:
            raise ValueError("Unexpected palette index count")
        pixels = []
        for y in range(height):
            for x in range(width):
                index = data[y*stride+x] if fmt in (9,11) else (
                    (data[y*stride+x//2] >> (4 if x%2 == 0 else 0)) & 15)
                pixels.append(rgba(fmt, palette[index]))
        return width, height, pixels

    bits = codec.Bits(blob, 1)
    fmt, width, height, method = bits.read(4), bits.read(8), bits.read(8), bits.read(4)
    if not width or not height or width*height > 8192:
        raise ValueError("Invalid texture dimensions")
    n = width*height
    if method in (5, 6, 7):
        rows = codec.decode_image(bits, fmt, width, height, method)
        return width, height, [rgba(fmt, p) for row in rows for p in row]
    if method in (0, 1):
        return width, height, [rgba(fmt, bits.read(codec.BITS_PER_PIXEL[fmt])) for _ in range(n)]
    channels, size = codec.NUM_CHANNELS[fmt], codec.CHANNEL_SIZES[fmt]
    predictor = bits.read(3) if method in (8, 9) else None
    if method in (2, 8):
        values = codec.inflate_huffman(bits, channels*n, size)
    elif method == 3:
        values = sum((codec.inflate_huffman(bits, n, size) for _ in range(channels)), [])
    elif method in (4, 9):
        values = codec.inflate_rle(bits, channels*n)
    else:
        raise ValueError(f"Unknown texture compression {method}")
    if predictor is not None:
        unfilter(values, width, predictor, size)
    alpha = [bits.read(1) for _ in range(n)] if codec.HAS_1BIT_ALPHA[fmt] else None
    pixels = []
    for i in range(n):
        c = [values[i+j*n] for j in range(channels)]
        if fmt in (0, 2):
            pixel = tuple(c[:3]) + ((c[3] if fmt == 0 else 255),)
        elif fmt in (1, 3):
            pixel = tuple(v*255//31 for v in c) + ((alpha[i]*255 if alpha else 255),)
        elif fmt in (4, 5):
            pixel = (c[0] * (17 if fmt == 5 else 1),)*3 + (c[1]*(17 if fmt == 5 else 1),)
        elif fmt == 6:
            pixel = (c[0]*255//7,)*3 + (alpha[i]*255,)
        elif fmt in (7, 8):
            pixel = (c[0]*(17 if fmt == 8 else 1),)*3 + (255,)
        else:
            raise ValueError(f"Unsupported channel format {fmt}")
        if any(v < 0 or v > 255 for v in pixel):
            raise ValueError("Invalid decoded channel")
        pixels.append(pixel)
    return width, height, pixels


def tiled_rgba(width, height, pixels):
    """PICA200 8x8 Morton order, ABGR bytes; flip rows for GL-style UVs.

    Non-power-of-two images are resampled, avoiding padded wrap seams.
    """
    w = max(8, 1 << (width-1).bit_length())
    h = max(8, 1 << (height-1).bit_length())
    result = bytearray(w*h*4)
    for y in range(h):
        for x in range(w):
            morton = sum(((x >> b)&1) << (2*b) | ((y >> b)&1) << (2*b+1) for b in range(3))
            offset = ((y//8)*(w//8)*64 + (x//8)*64 + morton)*4
            r,g,b,a = pixels[(height-1-y*height//h)*width+x*width//w]
            result[offset:offset+4] = bytes((a,b,g,r))
    return w, h, result
