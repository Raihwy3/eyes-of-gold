"""Keep fixed N64 address tokens out of the 3DSX pointer relocation tables."""
import struct
from pathlib import Path


def prepare_package_elf(source, destination):
    data = bytearray(Path(source).read_bytes())
    assert data[:7] == b'\x7fELF\x01\x01\x01'
    phoff, shoff = struct.unpack_from('<II', data, 28)
    phsize, phnum, shsize, shnum = struct.unpack_from('<4H', data, 42)
    loads = [struct.unpack_from('<8I', data, phoff+i*phsize) for i in range(phnum)]
    loads = [p for p in loads if p[0] == 1]
    low = min(p[2] for p in loads)
    high = max(p[2]+p[5] for p in loads)
    sections = [struct.unpack_from('<10I', data, shoff+i*shsize) for i in range(shnum)]
    removed = []
    for reloc in sections:
        if reloc[1] != 9 or not sections[reloc[7]][2] & 2:
            continue
        symbols = sections[reloc[6]]
        strings = sections[symbols[6]]
        for pos in range(reloc[4], reloc[4]+reloc[5], 8):
            address, info = struct.unpack_from('<II', data, pos)
            symbol = struct.unpack_from('<IIIBBH', data, symbols[4]+(info >> 8)*symbols[9])
            if info & 255 not in (2, 38):
                continue
            target = sections[reloc[7]]
            value = struct.unpack_from('<I', data, target[4]+address-target[3])[0]
            if (symbol[3] >> 4 == 2 and symbol[1] == 0) or low <= value <= high:
                continue
            name_start = strings[4]+symbol[0]
            name = data[name_start:data.index(0, name_start)].decode()
            if symbol[5] != 0xfff1:
                raise ValueError(f'Out-of-image pointer relocation: {name} {value:#x}')
            # The linked word is already the correct fixed numeric value. NONE
            # preserves it while preventing 3dsxtool from rebasing it as a pointer.
            struct.pack_into('<I', data, pos+4, info & ~255)
            removed.append((name, value))
    Path(destination).write_bytes(data)
    return removed


def validate_3dsx(path):
    """Check the emitted relocation targets, including fixed-token regressions."""
    data = Path(path).read_bytes()
    if data[:4] != b'3DSX':
        raise ValueError('Invalid 3DSX magic')
    header_size, reloc_size = struct.unpack_from('<HH', data, 4)
    if header_size < 32 or reloc_size != 8:
        raise ValueError('Unsupported 3DSX header')
    sizes = list(struct.unpack_from('<4I', data, 16))
    if sizes[3] > sizes[2]:
        raise ValueError('Invalid BSS size')
    counts = [struct.unpack_from('<II', data, header_size+i*reloc_size) for i in range(3)]
    cursor = header_size+3*reloc_size
    segments = []
    for size in (sizes[0], sizes[1], sizes[2]-sizes[3]):
        if cursor+size > len(data):
            raise ValueError('Truncated 3DSX segment')
        segments.append(data[cursor:cursor+size])
        cursor += size
    limit = sum((size+4095) & ~4095 for size in sizes[:3])
    checked = 0
    for segment, tables in zip(segments, counts):
        for kind, count in enumerate(tables):
            word = 0
            for _ in range(count):
                skip, patch = struct.unpack_from('<HH', data, cursor)
                cursor += 4
                word += skip
                if (word+patch)*4 > len(segment):
                    raise ValueError('Relocation source outside initialized segment')
                for at in range(word, word+patch):
                    value = struct.unpack_from('<I', segment, at*4)[0]
                    target = value & 0x0fffffff if kind else value
                    if target > limit:
                        raise ValueError(f'Relocation target outside image: {value:#x}')
                    checked += 1
                word += patch
    return checked
