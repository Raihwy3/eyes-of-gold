"""Supported retail stages and structural file/room traversal, without payloads."""
import csv,struct
from pathlib import Path
import build
STAGES=('dam','ark','run','sevx','sevbunker','silo','dest','sevxb','sevb','statue','arch','pete','depo','tra','jun','control','cave','crad','azt','cryp')
MULTIPLAYER_STAGES=('dish','ref','oat','ame','imp','ash','ark','sevb','arch','cave','cryp')

def background_name(stage):
    return {'sevbunker':'sev','sevxb':'sevx','statue':'stat','control':'arec','imp':'ame','ash':'ame'}.get(stage,stage)

def files():
    return {Path(row[2]).stem:tuple(map(int,row[:2]))
            for row in csv.reader((build.UP/'scripts/filelist.u.csv').open())}

def conversions():
    import native_game_assets as game
    seen=set()
    for stage in STAGES:
        background=background_name(stage)
        if background not in seen:
            seen.add(background)
            yield 'bg_'+background+'_all_p',game.convert_bg,False
            yield 'Tbg_'+background+'_all_p_stanZ',game.convert_stan,True
        yield 'Usetup'+stage+'Z',game.convert_setup,True
    for stage in MULTIPLAYER_STAGES:
        background=background_name(stage)
        if background not in seen:
            seen.add(background)
            yield 'bg_'+background+'_all_p',game.convert_bg,False
            yield 'Tbg_'+background+'_all_p_stanZ',game.convert_stan,True
        yield 'Ump_setup'+stage+'Z',game.convert_setup,True

def backgrounds(rom):
    table=files();seen=set()
    for stage in STAGES+MULTIPLAYER_STAGES:
        background=background_name(stage)
        if background in seen:continue
        seen.add(background)
        offset,size=table['bg_'+background+'_all_p'];bg=rom[offset:offset+size]
        start=struct.unpack_from('>I',bg,4)[0]&0xffffff
        # Match bg.c: count non-null PRIMARY pointers from record one.
        # The final non-null record delimits compressed streams; it is not a
        # renderable room. Runtime renders IDs 1 .. g_MaxNumRooms-1.
        stop=0
        for index in range(1,256):
            assert start+24*index+12<=len(bg),(stage,index)
            primary=struct.unpack_from('>I',bg,start+24*index+4)[0]
            if not primary:break
            assert primary>>24==15 and (primary&0xffffff)<len(bg),(stage,index,primary)
            stop+=1
        assert 1<stop<256,(stage,stop)
        yield stage,offset,size,bg,start,stop

def rooms(rom):
    for stage,_,_,bg,table,stop in backgrounds(rom):
        for room in range(1,stop):yield stage,room,bg,table
