"""Private ROM fixture for original-engine ABI validation, not converted assets."""
import csv
import hashlib
import re
import struct
import zlib

US_SHA1 = 'abe01e4aeb033b6c0836819f549c791b26cfde83'

def fnv(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value

def canonical_rom(path):
    data = bytearray(path.read_bytes())
    magic = data[:4]
    if magic == b'\x37\x80\x40\x12':
        data[::2], data[1::2] = data[1::2], data[::2]
    elif magic == b'\x40\x12\x37\x80':
        data[0::4],data[1::4],data[2::4],data[3::4] = data[3::4],data[2::4],data[1::4],data[0::4]
    elif magic != b'\x80\x37\x12\x40':
        raise ValueError('Unrecognized N64 ROM byte order')
    if hashlib.sha1(data).hexdigest() != US_SHA1:
        raise ValueError('Native target currently requires the US retail ROM')
    return data

def asset_fixture(rom,upstream,path):
    symbols={name:int(value,16) for name,value in re.findall(r'^\.set (\w+), 0x([0-9a-fA-F]+)',(upstream/'port/src/romassets_u.s').read_text(),re.M)}
    with (upstream/'scripts/filelist.u.csv').open() as stream:
        files=list(csv.reader(stream))
    def file(name):
        row=next(row for row in files if row[2].rsplit('/',1)[-1] in (name,name+'.bin'))
        offset,size=map(int,row[:2]);return 0x10000000+offset,bytes(rom[offset:offset+size])
    def segment(name):
        start,end=symbols['_'+name+'SegmentRomStart'],symbols['_'+name+'SegmentRomEnd']
        return start,bytes(rom[start-0x10000000:end-0x10000000])
    offsets={name:int(value,0) for name,value in re.findall(r'^#define PTR_ANIM_(\w+)\s+(0x[\da-fA-F]+|\d+)\s*$',(upstream/'assets/animationtable_data.h').read_text(),re.M)}
    table_text=(upstream/'src/game/initanitable.c').read_text()
    animations=[]
    for table in ('animation_table_ptrs1','animation_table_ptrs2'):
        body=re.search(r's32 '+table+r'\[\] = \{(.*?)\};',table_text,re.S).group(1)
        animations += [offsets[name] for name in re.findall(r'PTR_ANIM_(\w+)',body) if offsets[name]!=1]
    def flip(out,offset,width):out[offset:offset+width]=out[offset:offset+width][::-1]
    entries=[]
    for kind,(address,packed) in enumerate((segment('fontbankgothic'),segment('fontzurichbold'),segment('animation_data'),file('UbriefdamZ'),file('LdamE'),segment('musicsampletbl')),1):
        compressed=kind in (4,5)
        raw=zlib.decompress(packed[2:],-15) if compressed else packed
        out=bytearray(raw)
        if kind in (1,2):
            assert len(raw)>=2932
            for offset in range(0,2932,4):flip(out,offset,4)
        elif kind==3:
            assert len(animations)==len(set(animations))
            descriptors=set()
            for base in animations:
                first,last=struct.unpack_from('>I',raw,base+8)[0],struct.unpack_from('>I',raw,base+16)[0]
                for offset,width in ((0,4),(4,2),(8,4),(12,2),(14,2),(16,4)):flip(out,base+offset,width)
                if first==last==0:continue
                assert 0<=first<last<=len(raw) and (last-first)%6==0,(base,first,last,len(raw))
                for offset in range(first,last,6):
                    assert offset not in descriptors;descriptors.add(offset)
                    flip(out,offset,2);flip(out,offset+4,2)
        elif kind==4:
            assert len(raw)==48
            for offset in range(0,48,2):flip(out,offset,2)
        elif kind==5:
            position=0
            while position+4<=len(raw):
                value=struct.unpack_from('>I',raw,position)[0]
                if value and not position<value<len(raw):break
                flip(out,position,4);position+=4
        elif kind==6:
            count=struct.unpack_from('>H',raw)[0]
            assert 4+8*count==len(raw)
            flip(out,0,2)
            for offset in range(4,len(raw),8):
                flip(out,offset,4);flip(out,offset+4,2);flip(out,offset+6,2)
        entries.append((kind,address,packed,bytes(out),int(compressed)))
    position=12+32*len(entries);header=bytearray(struct.pack('<8sI',b'EOGAST01',len(entries)));payload=bytearray();manifest=[]
    for kind,address,packed,out,compressed in entries:
        header+=struct.pack('<8I',kind,address,position,len(packed),len(out),fnv(packed),fnv(out),compressed)
        payload+=packed;position+=len(packed)
        manifest.append({'kind':kind,'cart':address,'packed_bytes':len(packed),'native_bytes':len(out),'native_fnv':fnv(out)})
    path.write_bytes(header+payload)
    return manifest

def create(rom_path,upstream,path,route_checksum=0):
    rom = canonical_rom(rom_path)
    with (upstream/'scripts/filelist.u.csv').open() as stream:
        row = next(row for row in csv.reader(stream) if row[2].endswith('/UsetupdamZ.bin'))
    offset,size = map(int,row[:2])
    packed = bytes(rom[offset:offset+size])
    if packed[:2] != b'\x11\x72':
        raise ValueError('Incorrect setup compression header')
    setup = zlib.decompress(packed[2:],-15)
    code = zlib.decompress(rom[0x21992:],-15)
    tables = [p for p in range(0,len(code)-144,4)
              if all(struct.unpack_from('>II',code,p+i*8)[1] == i and
                     0x80000000 < struct.unpack_from('>I',code,p+i*8)[0] < 0x80080000
                     for i in range(18))]
    if len(tables) != 1:
        raise ValueError('Original global AI table is not unique')
    text = (upstream/'src/bondaicommands.h').read_text()
    ids = {name:int(value,0) for name,value in re.findall(r'#define\s+(\w+)_ID\s+(0x[\da-fA-F]+)',text)}
    lengths = {ids[name]:int(value,0) for name,value in re.findall(r'#define\s+(\w+)_LENGTH\s+(0x[\da-fA-F]+)',text) if name in ids}
    start = struct.unpack_from('>I',code,tables[0]+6*8)[0] - 0x80020d90
    end = start
    while True:
        command = code[end]
        end += code.index(0,end+1)-end+1 if command == 0xad else lengths[command]
        if command == 4:
            break
        if end-start > 8192:
            raise ValueError('Unterminated original attack script')
    attack = code[start:end]
    if len(attack) != 88:
        raise ValueError('Unexpected original attack script')
    seed,draws = 0xAB8D9F7781280783,bytearray()
    mask = (1<<64)-1
    for _ in range(1024):
        mixed = (((seed<<63)&mask)>>31) | (((seed<<31)&mask)>>32)
        mixed ^= ((seed<<44)&mask)>>32
        seed = mixed ^ ((mixed>>20)&0xfff)
        draws += struct.pack('<I',seed&0xffffffff)
    header = (1,len(packed),len(setup),fnv(setup),route_checksum,len(attack),fnv(attack),fnv(draws))
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(struct.pack('<8s8I',b'EOGNATV1',*header)+packed)
    assets=asset_fixture(rom,upstream,path.with_name('native-assets.dat'))
    return {'rom_sha1':US_SHA1,'compressed_setup_bytes':len(packed),'setup_bytes':len(setup),
            'setup_fnv':fnv(setup),'routes_fnv':route_checksum,'attack_script_bytes':len(attack),
            'attack_script_fnv':fnv(attack),'rng_fnv':fnv(draws),'asset_fixtures':assets}
