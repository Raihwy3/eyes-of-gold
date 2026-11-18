"""Predecode both retail sample banks into exact predictor-context PCM frames.

Notes, pitch, gain, positional audio and reverb keep their original live state.
A context key includes the complete predictor book, encoded frame and history;
loop-reset histories are baked separately so loop playback is not approximated.
"""
from pathlib import Path
import hashlib,json,struct
import build
from native_fixture import canonical_rom,fnv

MAGIC=b'EOGPCM2\0'

def signed(n,bits):
    n&=(1<<bits)-1
    return n-(1<<bits) if n>>(bits-1) else n

def decode_frame(frame,book,history):
    shift,index=frame[0]>>4,frame[0]&15
    assert index*16+16<=len(book)
    table=book[index*16:index*16+16];out=[]
    for half in range(2):
        residual=[]
        for byte in frame[1+4*half:5+4*half]:
            residual.extend(signed(signed(n,4)<<shift,16) for n in (byte>>4,byte&15))
        prev2,prev1=(history+out)[-2:]
        for j in range(8):
            acc=table[j]*prev2+table[8+j]*prev1+(residual[j]<<11)
            acc+=sum(table[8+j-k-1]*residual[k] for k in range(j))
            value=signed(acc,32)>>11
            out.append(max(-32768,min(32767,value)))
    return out

def generate():
    rompath=build.ROOT/'local/roms/GoldenEye.n64';out=build.BUILD/'audio-pcm';out.mkdir(exist_ok=True)
    target=build.BUILD/'native-audio.pcm';stamp=out/'stamp'
    digest=hashlib.sha256(Path(__file__).read_bytes()+str((rompath.stat().st_size,rompath.stat().st_mtime_ns)).encode()).hexdigest()
    if target.exists() and stamp.exists() and stamp.read_text()==digest:return
    rom=canonical_rom(rompath);books=[];book_ids={};records={};waves=0;raw=0;loop_waves=0;source_bytes=0
    for bank_start,bank_end,table in ((0x2ebde0,0x2f19a0,0x2f19a0),(0x3b4450,0x3b87f0,0x3b87f0)):
        bank=rom[bank_start:bank_end];seen=set();wave_offsets=set()
        u32=lambda p:struct.unpack_from('>I',bank,p)[0]
        u16=lambda p:struct.unpack_from('>H',bank,p)[0]
        for b in range(u16(2)):
            bn=u32(4+4*b)
            for i in range(u16(bn)+1):
                inst=u32(bn+8+4*i)
                if not inst:continue
                for sound in range(u16(inst+14)):
                    wave_offsets.add(u32(u32(inst+16+4*sound)+8))
        for wave in sorted(wave_offsets):
            base,length=u32(wave),u32(wave+4);kind=bank[wave+8]
            if kind==1:raw+=1;continue # Already PCM; the original endian bridge handles it.
            assert kind==0
            book_at=u32(wave+16);order,predictors=u32(book_at),u32(book_at+4)
            assert order==2 and 0<predictors<=8
            coefficients=struct.unpack_from('>'+str(16*predictors)+'h',bank,book_at+8)
            book_bytes=struct.pack('<'+str(len(coefficients))+'h',*coefficients)
            if book_bytes not in book_ids:book_ids[book_bytes]=len(books);books.append(book_bytes)
            book_id=book_ids[book_bytes]
            data=bytes(rom[table+base:table+base+length//9*9])
            assert len(data)==length//9*9
            if (book_id,data,u32(wave+12)) in seen:continue
            seen.add((book_id,data,u32(wave+12)));waves+=1;source_bytes+=len(data)
            def bake(first,history):
                for at in range(first,len(data),9):
                    frame=data[at:at+9];prev2,prev1=history[-2:]
                    pcm=decode_frame(frame,coefficients,history)
                    key=struct.pack('<Hhh',book_id,prev2,prev1)+frame
                    value=struct.pack('<16h',*pcm)
                    assert key not in records or records[key]==value
                    records[key]=value;history=pcm
            bake(0,[0]*16)
            loop=u32(wave+12)
            if loop and u32(loop+8):
                start,end=u32(loop),u32(loop+4)
                assert start<end<=len(data)//9*16
                state=list(struct.unpack_from('>16h',bank,loop+12))
                # alAdpcmPull restores the authored start-frame history, then
                # decodes beginning at the following compressed frame.
                bake((start//16+1)*9,state);loop_waves+=1
    payload=bytearray()
    for b in books:payload+=struct.pack('<I',len(b))+b+bytes(256-len(b))
    for key,pcm in sorted(records.items()):payload+=key+pcm+b'\0' # 48 bytes, no implicit ABI padding.
    header=MAGIC+struct.pack('<6I',2,len(books),len(records),len(payload),fnv(payload),0)
    temporary=target.with_suffix('.tmp');temporary.write_bytes(header+payload);temporary.replace(target)
    report=dict(adpcm_waves=waves,raw_pcm_waves=raw,loop_waves=loop_waves,books=len(books),frames=len(records),source_bytes=source_bytes,pcm_file_bytes=target.stat().st_size,ram_bytes=len(payload))
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');stamp.write_text(digest)
    print('Preprocessed all retail sample banks:',report)
if __name__=='__main__':build.prepare();generate()
