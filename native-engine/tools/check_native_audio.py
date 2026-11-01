#!/usr/bin/env python3
"""Retail RLE overwrite regression and complete native audio scalar contract."""
import ctypes,struct
import build,build_probe

def main():
    out=build.BUILD/'game-assets-host';out.mkdir(exist_ok=True)
    lib=out/'native-media.so'
    build_probe.run(['gcc','-shared','-fPIC','-Wall','-Wextra','-Werror','-I'+str(build.TARGET/'include'),
                     build.TARGET/'src/native_rle.c',build.TARGET/'src/native_audio_bank.c','-o',lib])
    api=ctypes.CDLL(str(lib));conv=api.native_audio_bank_convert
    conv.argtypes=[ctypes.c_void_p,ctypes.c_uint32];conv.restype=ctypes.c_int
    rom=(build.BUILD/'native-cart.bin').read_bytes()
    for a,z,table in [(0x2ebde0,0x2f19a0,0x102f19a0),(0x3b4450,0x3b87f0,0x103b87f0)]:
        original=rom[a:z];expected=bytearray(original);seen=set();fields=set();raw=[]
        def swap(o,size):
            assert o+size<=len(original)
            fields.add((o,size));expected[o:o+size]=original[o:o+size][::-1]
            return int.from_bytes(original[o:o+size],'big')
        def walk(o,kind):
            if not o and kind!='file':return
            if (o,kind) in seen:return
            seen.add((o,kind))
            if kind=='file':
                swap(o,2);count=swap(o+2,2);base=4;child='bank'
            elif kind=='bank':
                count=swap(o,2);swap(o+4,4);walk(swap(o+8,4),'inst');base=12;child='inst'
            elif kind=='inst':
                swap(o+12,2);count=swap(o+14,2);base=16;child='sound'
            elif kind=='sound':
                for pos,k in [(0,'env'),(4,'key'),(8,'wave')]:walk(swap(o+pos,4),k)
                return
            elif kind=='wave':
                offset=swap(o,4);length=swap(o+4,4)
                if original[o+8]==0:
                    walk(swap(o+12,4),'loop');walk(swap(o+16,4),'book')
                else:
                    assert original[o+8]==1;raw.append((table+offset,length));walk(swap(o+12,4),'rawloop')
                return
            elif kind in ('env','loop','rawloop'):
                for pos in (0,4,8):swap(o+pos,4)
                if kind=='loop':
                    for pos in range(12,44,2):swap(o+pos,2)
                return
            elif kind=='book':
                order=swap(o,4);n=swap(o+4,4)
                for pos in range(8,8+16*order*n,2):swap(o+pos,2)
                return
            else:assert kind=='key';return
            for i in range(count):walk(swap(o+base+4*i,4),child)
        walk(0,'file')
        buf=ctypes.create_string_buffer(original);assert conv(buf,len(original))==1
        assert buf.raw[:len(original)]==expected
        api.native_audio_register_raw.argtypes=[ctypes.c_void_p,ctypes.c_uint32]
        assert api.native_audio_register_raw(buf,table)==1
        for address,length in raw:
            api.native_audio_swap_samples.argtypes=[ctypes.c_uint32,ctypes.c_void_p,ctypes.c_uint32]
            payload=rom[address-0x10000000:address-0x10000000+length]
            data=ctypes.create_string_buffer(payload)
            api.native_audio_swap_samples(address,data,length)
            assert data.raw[:length]==b''.join(payload[i:i+2][::-1] for i in range(0,length,2))
        for truncated in (0,3,8,len(original)//2):
            bad=ctypes.create_string_buffer(original[:truncated]);assert conv(bad,truncated)==0
        print(f'PASS retail audio bank {a:06x}: {len(seen)} shared nodes, {len(fields)} scalar fields, {len(raw)} raw PCM waves')
    encoded=rom[0x2a4d50:0x2bf2d0]
    w,h=struct.unpack_from('>HH',encoded);assert (w,h)==(440,299)
    wrongw,wrongh=struct.unpack_from('<HH',encoded)
    assert wrongw*wrongh>0x40400
    prepare=api.native_rle_prepare;prepare.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_size_t]
    data=ctypes.create_string_buffer(encoded);assert prepare(data,len(encoded),0x40400)
    assert struct.unpack_from('<HH',data.raw)==(w,h)
    for content,capacity in [(encoded[:10],0x40400),(encoded,w*h-1),(encoded[:10]+b'\0\xff',0x40400)]:
        bad=ctypes.create_string_buffer(content);assert not prepare(bad,len(content),capacity)
        assert bad.raw[:len(content)]==content
    # Exercise the actual decoder, with a guard region beyond the image.
    obj=out/'rle-original.o'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-fPIC','-c',build.UP/'src/game/rle.c','-o',obj])
    decoder=out/'rle-original.so';build_probe.run(['gcc','-shared',obj,'-o',decoder])
    decode=ctypes.CDLL(str(decoder)).rle_expand_8bit;decode.argtypes=[ctypes.c_void_p,ctypes.c_void_p]
    pixels=ctypes.create_string_buffer(b'\xa5'*(w*h+64));decode(data,pixels)
    assert pixels.raw[w*h:w*h+64]==b'\xa5'*64
    expected=bytearray();pos=10
    while len(expected)<w*h:
        count,value=encoded[pos:pos+2];expected.extend(bytes([value])*count);pos+=2
    assert pixels.raw[:w*h]==expected
    print(f'PASS original intro RLE: incorrect dimensions requested {wrongw*wrongh} bytes; decoded {w*h} with intact guard')
if __name__=='__main__':
    build.prepare();build_probe.host_runner();main()
