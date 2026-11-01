"""Bake original triangle programs, preserving all material/visibility ordering."""
from pathlib import Path
import csv, hashlib, json, struct, zlib
import build,stage_assets
from native_fixture import canonical_rom

def generate():
    out=build.BUILD/'room-packets';out.mkdir(exist_ok=True)
    header=build.BUILD/'include/native_room_packets_data.h'
    rompath=build.ROOT/'local/roms/GoldenEye.n64'
    digest=hashlib.sha256(Path(__file__).read_bytes()+Path(stage_assets.__file__).read_bytes()+str((rompath.stat().st_size,rompath.stat().st_mtime_ns)).encode()).hexdigest()
    if header.exists() and (out/'stamp').exists() and (out/'stamp').read_text()==digest:return
    rom=canonical_rom(rompath)
    runs=set();total=0
    for stage,room,bg,table in stage_assets.rooms(rom):
        for pointer in struct.unpack_from('>III',bg,table+room*24)[1:]:
            if not pointer:continue
            dl=zlib.decompress(bg[(pointer&0xffffff)+2:],-15);words=[]
            def flush():
                nonlocal words,total
                if words:runs.add(tuple(words));total+=1;words=[]
            for at in range(0,len(dl)-7,8):
                x,y=struct.unpack_from('>II',dl,at)
                if x>>24 in (0xbf,0xb1):
                    if len(words)==32:flush()
                    words.append((x,y))
                else:flush()
                if x>>24==0xb8:break
            flush()
    records=[]
    for words in sorted(runs):
        triangles=[]
        for x,y in words:
            if x>>24==0xbf:triangles.append(tuple((y>>s&255)//10 for s in (16,8,0)))
            else:triangles.extend((y>>(8*j)&15,y>>(8*j+4)&15,x>>(4*j)&15) for j in range(4) if (y>>(8*j)&255)|(x>>(4*j)&15))
        assert len(triangles)<=128 and all(v<128 for t in triangles for v in t)
        records.append('{0,'+str(len(words))+','+str(len(triangles))+',{'+','.join('{'+str(x)+'u,'+str(y)+'u}' for x,y in words)+'},{'+','.join('{'+','.join(map(str,t))+'}' for t in triangles)+'}}')
    header.write_text('static const NativeSceneryBatches::Entry native_room_packets[]={'+','.join(records)+'};\nstatic const unsigned native_room_packet_count='+str(len(records))+';\n')
    (out/'report.json').write_text(json.dumps(dict(runs=total,unique=len(records),bytes=len(records)*652),indent=2)+'\n')
    (out/'stamp').write_text(digest)
    print('Offline room triangle packets:',total,'runs,',len(records),'unique')

def apply(text):
    needle='static unsigned native_run_scenery_batch(Gfx *cmd) {'
    helper='''
#include "native_room_packets_runtime.h"
static NativeRoomPackets native_room_programs;
'''
    assert text.count(needle)==1;text=text.replace(needle,helper+needle)
    a=text.index('    NativeSceneryBatches::Word words[NativeSceneryBatches::max_commands];',text.index(needle))
    b=text.index('    // Validate before touching RSP cache entries.',a)
    original=text[a:b]
    original=original.replace('    const auto &entry=native_scenery_batches.get(words,count,(uint8_t)G_TRI1,(uint8_t)G_TRI4);','    selected=&native_scenery_batches.get(words,count,(uint8_t)G_TRI1,(uint8_t)G_TRI4);')
    text=text[:a]+'''    unsigned count=0;
    const NativeSceneryBatches::Entry *selected=nullptr;
    static const bool native_packets_enabled=getenv("EOG_DISABLE_ROOM_PACKETS")==nullptr;
    if(native_scenery_kind==1 && native_packets_enabled)selected=native_room_programs.get(cmd,count);
    if(!selected) {
'''+original.replace('unsigned count=0;','')+'''    }
    const auto &entry=*selected;
'''+text[b:]
    needle='            case NATIVE_SCENERY_COMMAND:'
    assert text.count(needle)==1
    text=text.replace(needle,'''            case NATIVE_ROOM_PACKET_COMMAND:
                native_room_programs.scope(C0(0,1)?(const NativeRoomPacketScope *)seg_addr(cmd->words.w1):nullptr);
                break;
'''+needle)
    needle='    gfx_rapi->start_frame();';assert text.count(needle)==1
    text=text.replace(needle,needle+'\n    native_room_programs.begin_frame();')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    text=text.replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Room packets: direct=%u bound=%u fallback=%u bytes=%u",native_room_programs.hits,native_room_programs.binds,native_room_programs.misses,(unsigned)sizeof native_room_programs);
'''+needle)
    return text
