#!/usr/bin/env python3
"""Exercise original graphics code against a recording backend, not a GPU."""
from pathlib import Path
import csv
import os
import struct
import sys
import build
import build_probe
from native_fixture import canonical_rom


def main():
    build.prepare()
    out=build.BUILD/'graphics-host';out.mkdir(exist_ok=True)
    # Host headers/reference setup are shared with the original-code checks.
    build_probe.host_runner()
    sanitize=['-fsanitize=address,undefined','-g','-fno-omit-frame-pointer'] if os.environ.get('EOG_GRAPHICS_SANITIZE') else []
    objects=[]
    for rel in ('src/game/title2.c','src/game/title.c','src/game/title3.c','src/game/blood_animation.c','assets/rarewarelogo.c','assets/font_dl.c',*[f'src/libultra/gu/{n}.c' for n in ('mtxutil','ortho','perspective','translate','scale','lookat','rotate','normalize')],'src/game/rle.c',*[f'src/game/{n}.c' for n in ('image','image_bank','initimages','tex','lightfixture','unk_092E50','lv','decompress','zlib','stan','stanintersection','bondhead','bondview2','model','initBondDATAdefaults','initplayergaitobject','initunk_0009E0','initanitable','matrixmath','player','objecthandler','matrixmath_misc','quaternion','math_floor','math_ceil','math_unk_05A9E0','bondview')],'src/ramrom.c','port/src/random.c','port/src/libultra.c','port/fast3d/gfx_pc.cpp','port/fast3d/gfx_cc.cpp'):
        cpp=rel.endswith('.cpp');flags=build_probe.engine_args(True)
        # ASan registration would retain unrelated firing-animation tables
        # from this getter-only dependency, defeating --gc-sections. Keep
        # its code checks; gait/model/global buffer checks remain enabled.
        if sanitize and rel=='src/game/bondview.c':flags+=['--param=asan-globals=0']
        if rel=='port/src/libultra.c':flags+=['-I'+str(build.BUILD/'include/libultra-system')]
        if cpp:
            flags=[f for f in flags if f not in ('-std=gnu11','-fplan9-extensions','-fpermissive') and not any(str(f).startswith('-D'+n+'=') for n in build.GAME_LIBC)]
            flags+=['-std=gnu++20','-fno-rtti','-fno-exceptions']
            if rel.startswith('port/fast3d/'):flags+=['-O2']
        obj=out/(rel.replace('/','__')+'.o')
        build_probe.run(['g++' if cpp else 'gcc',*flags,*sanitize,'-I'+str(build.UP/'port/fast3d'),'-c',build.source_path(Path(rel)),'-o',obj])
        objects.append(obj)
    for name in ('graphics_scene.c','native_room.c','native_motion.c','baseline_options.c','native_settings.c','native_controls.c','gfx_native.cpp','graphics_host_check.cpp','pica_inputs.cpp'):
        cpp=name.endswith('.cpp');flags=build_probe.engine_args(True)
        if name=='graphics_host_check.cpp':flags=['-std=gnu++20','-O1','-fno-pie','-ffunction-sections','-fdata-sections','-I'+str(build.TARGET/'include')]
        elif cpp:flags=[f for f in flags if f not in ('-std=gnu11','-fplan9-extensions','-fpermissive')]+['-std=gnu++20']
        obj=out/(name+'.o')
        build_probe.run(['g++' if cpp else 'gcc',*flags,*sanitize,'-I'+str(build.UP/'port/fast3d'),'-c',build.TARGET/'src'/name,'-o',obj])
        objects.append(obj)
    intro_obj=out/'intro-scene.o'
    build_probe.run(['gcc',*build_probe.engine_args(True),*sanitize,'-c',build.TARGET/'tests/intro_scene_host.c','-o',intro_obj])
    objects.append(intro_obj)
    objects += [build.BUILD/'host/native_ram.o',build.BUILD/'host/native_cart.o',build.BUILD/'host/romassets.o',build.BUILD/'host/runtime_symbols.o',build.BUILD/'host/game_memory.o',build.BUILD/'host/rom_fixups.o']
    binary=out/'graphics-check'
    build_probe.run(['g++',*sanitize,'-no-pie','-Wl,--gc-sections,-Ttext-segment=0x02000000',*objects,'-pthread','-lm','-o',binary])
    from room_fixture import create, create_world, create_walk_reference, create_motion
    create(build.ROOT/'local/roms/GoldenEye.n64',build.UP,build.BUILD/'native-room.dat')
    create_world(build.ROOT/'local/roms/GoldenEye.n64',build.UP,build.BUILD/'native-world.dat')
    create_walk_reference(build.ROOT/'local/roms/GoldenEye.n64',build.BUILD/'world-test-route.dat')
    create_motion(build.ROOT/'local/roms/GoldenEye.n64',build.BUILD/'native-motion.dat')
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64')
    with (build.UP/'scripts/filelist.u.csv').open() as f:row=next(r for r in csv.reader(f) if 'usedby7F008DE4.bin' in r[2])
    start,size=map(int,row[:2]);encoded=bytes(rom[start:start+size]);raw=bytearray()
    for pos in range(10,len(encoded),2):
        count,value=encoded[pos:pos+2];assert count
        raw+=bytes([value])*count
        if len(raw)>=440*299:break
    assert len(raw)==440*299
    # Independent Python texture decoding is validation-only. Neither this
    # utility nor any recreation mesh/texture asset is linked or packaged.
    # Independent decoder is kept beside this native reference check.
    sys.path.insert(0,str(build.UP/'tools_pc'))
    from texture_pixels import decode
    import texdecode
    from native_fixture import fnv
    image_rows=list(csv.reader((build.UP/'imagelist.u.csv').open()))
    import json
    texture_ids=json.loads((build.BUILD/'native-room.json').read_text())['texture_ids']
    expected=bytearray()
    for tid in texture_ids:
        offset,size=map(int,image_rows[tid][:2]);w,h,pixels=decode(rom[offset:offset+size],texdecode)
        expected+=struct.pack('<4I',tid,w,h,fnv(bytes(c for pixel in pixels for c in pixel[:3])))
    (out/'expected-room-pixels.dat').write_bytes(expected)
    # Validate every texture the walking test can upload against an independent decoder.
    import zlib
    bg=rom[0x5ffc50:0x5ffc50+197024];ids=set()
    for room in range(1,137):
        for pointer in struct.unpack_from('>III',bg,20+room*24)[1:]:
            if not pointer: continue
            stream=zlib.decompress(bg[(pointer&0xffffff)+2:],-15)
            for pos in range(0,len(stream)-7,8):
                a,b=struct.unpack_from('>II',stream,pos)
                if a>>24==0xc0: ids.add(b&4095)
                if a>>24==0xb8: break
    expected=bytearray()
    for tid in sorted(ids):
        offset,size=map(int,image_rows[tid][:2]);w,h,pixels=decode(rom[offset:offset+size],texdecode)
        expected+=struct.pack('<4I',tid,w,h,fnv(bytes(c for pixel in pixels for c in pixel[:3])))
    (build.BUILD/'expected-world-pixels.dat').write_bytes(expected)
    (out/'native-menu.rle').write_bytes(encoded);(out/'expected-menu.raw').write_bytes(raw)
    env={k:v for k,v in os.environ.items() if not k.startswith('GE_')};env['GE_NOSHADERWARM']='1'
    if sanitize:env['ASAN_OPTIONS']='detect_stack_use_after_return=0:detect_leaks=0'
    env.pop('EOG_DISABLE_RSP_PREP_CACHE',None)
    env.pop('EOG_DISABLE_UV_PREP',None)
    env.pop('EOG_DISABLE_FOG_FASTPATH',None)
    env.pop('EOG_DISABLE_SCENERY_CACHE',None)
    env.pop('EOG_DISABLE_SCENERY_BATCHES',None)
    env.pop('EOG_DISABLE_OBJECT_BATCHES',None)
    env.pop('EOG_DISABLE_MESH_LOD',None)
    env.pop('EOG_TEST_GENERATED_LOD',None)
    env.pop('EOG_TEST_GPU_WORLD',None)
    env.pop('EOG_DISABLE_GPU_WORLD',None)
    env.pop('EOG_DISABLE_EARLY_WORLD_CULL',None)
    env.pop('EOG_DISABLE_CLIP_COLOUR_FASTPATH',None)
    env['EOG_ENABLE_GPU_WORLD']='1'
    env['EOG_GRAPHICS_TRACE']=str(out/'cached.trace')
    result=build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    import re
    packets=re.search(r'Room packet totals: direct=(\d+) bound=(\d+)',result.stdout)
    assert packets and int(packets[1])>0 and int(packets[2])>0
    packet_env=dict(env);packet_env['EOG_DISABLE_ROOM_PACKETS']='1';packet_env['EOG_GRAPHICS_TRACE']=str(out/'room-packets-reference.trace')
    build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=packet_env,timeout=30)
    assert (out/'cached.trace').read_bytes()==(out/'room-packets-reference.trace').read_bytes(), 'Offline room packets changed ordered graphics output'
    result.stdout+=f'PASS room packet/reference: complete ordered geometry/material/texture trace matches; {packets[1]} command-run scans bypassed, {packets[2]} bindings validated\n'
    clips=re.search(r'CPU clip totals: avoidedFallback=(\d+) fastColourRounds=(\d+)',result.stdout)
    assert clips and int(clips[1])>0 and int(clips[2])>0
    env['EOG_DISABLE_EARLY_WORLD_CULL']='1';env['EOG_DISABLE_CLIP_COLOUR_FASTPATH']='1'
    env['EOG_GRAPHICS_TRACE']=str(out/'clip-reference.trace')
    build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    assert (out/'cached.trace').read_bytes()==(out/'clip-reference.trace').read_bytes(), 'Early clipping rejection or colour rounding changed submitted graphics'
    result.stdout+=f'PASS CPU clipping/reference: identical ordered draw/vertex/material trace; avoided {clips[1]} slow fallback calls and {clips[2]} floorf colour rounds\n'
    env.pop('EOG_DISABLE_EARLY_WORLD_CULL');env.pop('EOG_DISABLE_CLIP_COLOUR_FASTPATH')
    env['EOG_DISABLE_RSP_PREP_CACHE']='1'
    env['EOG_GRAPHICS_TRACE']=str(out/'uncached.trace')
    baseline=build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    assert (out/'cached.trace').read_bytes()==(out/'uncached.trace').read_bytes(), 'Triangle preparation cache changed submitted graphics'
    result.stdout+='PASS cached/uncached Fast3D: identical vertex bytes, shader IDs, textures and render-state trace\n'
    material_env=dict(env);material_env.pop('EOG_DISABLE_RSP_PREP_CACHE',None)
    material_env['EOG_DISABLE_MATERIAL_NOOPS']='1';material_env['EOG_DISABLE_RECT_PAIR_CACHE']='1'
    material_env['EOG_GRAPHICS_TRACE']=str(out/'material-work-reference.trace')
    material_reference=build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=material_env,timeout=30)
    assert (out/'cached.trace').read_bytes()==(out/'material-work-reference.trace').read_bytes(), 'Material no-op/rectangle reuse changed graphics'
    work=re.findall(r'Material work avoided: stateNoops=(\d+) rectanglePairs=(\d+)',result.stdout)
    assert sum(int(a) for a,b in work)>0
    result.stdout+='PASS material work/reference: complete ordered draw/vertex/texture/state trace matches with no-op skipping and rectangle pair reuse disabled\n'
    quad_env=dict(material_env);quad_env['EOG_DISABLE_QUAD_BATCHES']='1'
    quad_env['EOG_GRAPHICS_TRACE']=str(out/'quad-reference.trace')
    build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=quad_env,timeout=30)
    assert (out/'cached.trace').read_bytes()==(out/'quad-reference.trace').read_bytes(), 'Direct rectangles changed graphics'
    quads=re.findall(r'Direct UI quads: batches=(\d+)',result.stdout);assert sum(map(int,quads))>0
    result.stdout+='PASS direct UI quad/reference: identical ordered vertices, materials, texture coordinates, clipping/scissor and frame output across menu/watch/HUD scenes\n'


    env.pop('EOG_DISABLE_RSP_PREP_CACHE',None)
    env['EOG_DISABLE_UV_PREP']='1'
    env['EOG_GRAPHICS_TRACE']=str(out/'uv-reference.trace')
    build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    assert (out/'cached.trace').read_bytes()==(out/'uv-reference.trace').read_bytes(), 'Prepared UVs changed submitted graphics'
    result.stdout+='PASS prepared/reference UVs: identical submitted geometry, textures and render state\n'

    env.pop('EOG_DISABLE_UV_PREP',None)
    env['EOG_DISABLE_FOG_FASTPATH']='1'
    env['EOG_GRAPHICS_TRACE']=str(out/'prior-cache.trace')
    prior=build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    assert (out/'cached.trace').read_bytes()==(out/'prior-cache.trace').read_bytes(), 'Fog fast path changed submitted graphics'
    import re
    model_counts=re.search(r'Model batch totals: actors=(\d+) vehicles=(\d+) normalsReused=(\d+) normalsBuilt=(\d+)',result.stdout)
    assert model_counts and all(int(model_counts[i])>0 for i in range(1,5))
    env['EOG_DISABLE_ACTOR_BATCHES']='1';env['EOG_DISABLE_LIGHT_NORMAL_CACHE']='1'
    env['EOG_GRAPHICS_TRACE']=str(out/'actor-reference.trace')
    build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    assert (out/'cached.trace').read_bytes()==(out/'actor-reference.trace').read_bytes(), 'Actor batching/normal reuse changed graphics'
    env.pop('EOG_DISABLE_ACTOR_BATCHES',None);env.pop('EOG_DISABLE_LIGHT_NORMAL_CACHE',None)
    result.stdout+=f'PASS actor/lighting reference: identical complete draw/vertex/material trace, {model_counts[1]} batched actor triangles, {model_counts[2]} vehicle triangles and {model_counts[3]} repeated normal calculations avoided\n'
    prepared=re.search(r'Prepared batch totals: hits=(\d+) builds=(\d+) corners=(\d+)',result.stdout)
    assert prepared and all(int(prepared[i])>0 for i in (1,2,3))
    env['EOG_DISABLE_PREPARED_BATCHES']='1';env['EOG_GRAPHICS_TRACE']=str(out/'prepared-reference.trace')
    build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    assert (out/'cached.trace').read_bytes()==(out/'prepared-reference.trace').read_bytes(), 'Prepared batches changed submitted graphics'
    env.pop('EOG_DISABLE_PREPARED_BATCHES',None)
    env['EOG_TEST_PREPARED_CHURN']='1';env['EOG_GRAPHICS_TRACE']=str(out/'prepared-churn.trace')
    churn=build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    assert (out/'cached.trace').read_bytes()==(out/'prepared-churn.trace').read_bytes(), 'Recycled material tokens changed prepared graphics'
    churn_counts=re.search(r'Prepared batch totals: hits=(\d+) builds=(\d+) corners=(\d+)',churn.stdout)
    assert churn_counts and int(churn_counts[1])>0
    env.pop('EOG_TEST_PREPARED_CHURN',None)
    result.stdout+=f'PASS stable prepared keys: {churn_counts[1]} hits with material slots cleared every frame, identical draw trace\n'

    result.stdout+=f'PASS prepared batch/reference: identical ordered draw/vertex/texture trace; {prepared[1]} hits bypass packing for {prepared[3]} corners\n'
    static_ids=re.search(r'Static identity totals: vertices=(\d+) reused=(\d+)',result.stdout)
    assert static_ids and int(static_ids[2])>0
    result.stdout+=f'PASS static attribute identities: immutable GPU attribute bytes verified for {static_ids[2]} reuses among {static_ids[1]} original-game corners, excluding current position/fog/LOD\n'
    ids=re.search(r'Vertex identity totals: reused=(\d+) identified=(\d+)',result.stdout)
    assert ids and int(ids[1])>0 and int(ids[2])>int(ids[1])
    result.stdout+=f'PASS vertex identities: complete packed bytes verified for {ids[1]} repeated corners among {ids[2]} identified original-game vertices\n'
    env['EOG_DISABLE_VERTEX_REFS']='1'
    env['EOG_GRAPHICS_TRACE']=str(out/'vertex-refs-reference.trace')
    build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    assert (out/'cached.trace').read_bytes()==(out/'vertex-refs-reference.trace').read_bytes(), 'Indirect packing changed graphics'
    env.pop('EOG_DISABLE_VERTEX_REFS',None)
    result.stdout+='PASS indirect packing/reference: identical ordered geometry, texture and material trace across original-game scenes\n'
    current_counts=re.search(r'Packed vertices: reused=(\d+) built=(\d+)',result.stdout)
    previous_counts=re.search(r'Packed vertices: reused=(\d+) built=(\d+)',prior.stdout)
    assert current_counts and previous_counts
    old_builds=int(previous_counts[2]);new_builds=int(current_counts[2])
    result.stdout+=f'PASS fog fast/reference path: identical output; packed vertices {old_builds} -> {new_builds} across host scenes\n'
    env.pop('EOG_DISABLE_FOG_FASTPATH',None)
    env['EOG_DISABLE_SCENERY_CACHE']='1'
    env['EOG_GRAPHICS_TRACE']=str(out/'scenery-reference.trace')
    reference=build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    assert (out/'cached.trace').read_bytes()==(out/'scenery-reference.trace').read_bytes(), 'Persistent scenery cache changed submitted graphics'
    current=re.search(r'Scenery attributes: reused=(\d+) persisted=(\d+) built=(\d+)',result.stdout)
    assert current and int(current[1])>0 and int(current[2])>0
    ref_counts=re.search(r'Packed vertices: reused=(\d+) built=(\d+)',reference.stdout)
    assert int(current_counts[2])<int(ref_counts[2]), 'Scenery cache did not reduce attribute packing'
    result.stdout+=f'PASS persistent scenery/reference: identical vertex bytes, shaders, textures and state; full vertex packs {ref_counts[2]} -> {current_counts[2]}, reused attributes {current[1]} (cross-frame {current[2]})\n'
    env.pop('EOG_DISABLE_SCENERY_CACHE',None)
    env['EOG_DISABLE_SCENERY_BATCHES']='1'
    env['EOG_GRAPHICS_TRACE']=str(out/'batch-reference.trace')
    build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    assert (out/'cached.trace').read_bytes()==(out/'batch-reference.trace').read_bytes(), 'Compiled scenery batches changed submitted graphics'
    batches=re.search(r'Scenery batches: reused=(\d+) compiled=(\d+) fast=(\d+) fallback=(\d+)',result.stdout)
    assert batches and all(int(batches[i])>0 for i in range(1,5))
    result.stdout+=f'PASS compiled scenery/reference: identical ordered draw/vertex/texture/state trace; avoided per-triangle dispatch for {batches[3]} triangles, cache hits {batches[1]}, clipping fallback {batches[4]}\n'
    env.pop('EOG_DISABLE_SCENERY_BATCHES',None)
    env['EOG_DISABLE_OBJECT_BATCHES']='1'
    env['EOG_GRAPHICS_TRACE']=str(out/'object-reference.trace')
    build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    assert (out/'cached.trace').read_bytes()==(out/'object-reference.trace').read_bytes(), 'Object batching changed submitted graphics'
    objects=re.search(r'Object batches: fast=(\d+) rejected=(\d+) fallback=(\d+)',result.stdout)
    assert objects and int(objects[1])>0
    result.stdout+=f'PASS object/reference: identical ordered vertex/texture/material/draw trace with lit and moving instances; {objects[1]} triangles bypassed individual dispatch\n'
    env.pop('EOG_DISABLE_OBJECT_BATCHES',None)
    env['EOG_TEST_GENERATED_LOD']='1'
    env['EOG_GRAPHICS_TRACE']=str(out/'mesh-lod.trace')
    lod_result=build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    assert 'PASS actual generated LOD tiers:' in lod_result.stdout
    result.stdout+='PASS actual generated LOD tiers: full/mid/far/distant BG and objects retain equal coverage with 32/24/18/14 triangles; texture tiers reset for UI\n'
    env['EOG_DISABLE_MESH_LOD']='1'
    build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    env.pop('EOG_DISABLE_MESH_LOD',None);env.pop('EOG_TEST_GENERATED_LOD',None)
    env['EOG_TEST_GPU_WORLD']='1'
    gpu=build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=env,timeout=30)
    assert 'PASS GPU world:' in gpu.stdout
    assert 'PASS original material proofs:' in gpu.stdout
    result.stdout+='PASS original material proofs: input RGB/alpha uniformity verified against all submitted vertices, including context changes and clipping\n'
    result.stdout+='PASS GPU world: model-space transform output and both hardware cull directions match reference; fallback restores UI state\n'
    model_env=dict(env);model_env.pop('EOG_ENABLE_GPU_WORLD',None);model_env.pop('EOG_TEST_GPU_WORLD',None)
    model_env['EOG_TEST_MODEL_GPU']='1';model_env['EOG_GRAPHICS_TRACE']=str(out/'default-model-gpu.trace')
    models=build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=model_env,timeout=30)
    assert 'PASS default GPU models:' in models.stdout
    retained_env=dict(model_env);retained_env.pop('EOG_TEST_MODEL_GPU',None)
    retained_env['EOG_TEST_RETAINED_GPU']='1';retained_env['EOG_ENABLE_GPU_WORLD']='1'
    retained_env['EOG_GRAPHICS_TRACE']=str(out/'retained-gpu.trace')
    retained=build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=retained_env,timeout=30)
    assert 'PASS retained GPU producer:' in retained.stdout
    assert 'PASS original fog near-plane GPU:' in retained.stdout
    assert 'PASS simple glass GPU:' in retained.stdout
    assert 'PASS GPU early visibility:' in retained.stdout
    assert 'PASS deferred background:' in retained.stdout
    assert 'PASS actual world winding:' in retained.stdout
    assert 'PASS early model faces:' in retained.stdout
    assert 'PASS reference background mode:' in retained.stdout
    assert 'PASS CPU room batch capacity:' in retained.stdout
    assert 'PASS moving CPU room cache:' in retained.stdout
    assert 'PASS deferred CPU rooms:' in retained.stdout
    result.stdout+='PASS moving CPU room cache: ordered geometry/attributes match unbatched CPU triangles across changing camera visibility, colours and clipping\n'
    result.stdout+='PASS CPU room batch capacity: 100 distinct CPU batches reuse exact output without rebuilding; pending indexing hints are consumed and cleared\n'
    result.stdout+='PASS reference background mode: CPU BG output matches original; prop/actor/vehicle GPU paths remain enabled\n'
    result.stdout+='PASS early model faces: actual ordered CPU/GPU attributes agree with no hardware winding rejection and hidden faces skip CPU packing\n'
    result.stdout+='PASS actual world winding: cancelled-sign world facets survive real interpreter culling in both windings/modes\n'
    deferred_reference_env=dict(retained_env);deferred_reference_env['EOG_DISABLE_DEFERRED_BG']='1'
    deferred_reference_env['EOG_GRAPHICS_TRACE']=str(out/'deferred-reference.trace')
    build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=deferred_reference_env,timeout=30)
    assert (out/'retained-gpu.trace').read_bytes()==(out/'deferred-reference.trace').read_bytes(), 'Deferred background changed ordered GPU submission attributes'
    cpu_reference_env=dict(retained_env);cpu_reference_env['EOG_DISABLE_DEFERRED_CPU_ROOMS']='1'
    cpu_reference_env['EOG_GRAPHICS_TRACE']=str(out/'cpu-room-eager-reference.trace')
    build_probe.run([binary,out/'native-menu.rle',out/'expected-menu.raw',build.BUILD/'native-room.dat',out/'expected-room-pixels.dat'],env=cpu_reference_env,timeout=30)
    assert (out/'retained-gpu.trace').read_bytes()==(out/'cpu-room-eager-reference.trace').read_bytes(), 'Deferred CPU room transforms changed complete ordered graphics output'
    result.stdout+='PASS deferred CPU rooms: exact eager/deferred ordered geometry/UV/shade/fog/material trace, all transforms avoided for hidden blocks, partial loads and 270-state budget fallback\n'
    result.stdout+='PASS deferred background: offscreen depth/fog work avoided with exact load-time state through partial loads and CPU fallback\n'
    result.stdout+='PASS GPU early visibility: Original LOD offscreen batches skip GPU/material setup and visible output matches reference\n'
    result.stdout+='PASS original fog near-plane GPU: 21 camera positions match original CPU geometry, shade and fog with custom distance fog disabled\n'
    result.stdout+='PASS simple glass GPU: original/simple distance tiers and five opacity values match full ordered CPU attributes with shader/texture restoration\n'
    result.stdout+='PASS retained GPU producer: repeated BG, props, actors and vehicles match complete ordered reference GPU vertex attributes with source/material mutation and clipping fallbacks\n'
    result.stdout+='PASS default GPU models: six actor/vehicle cases match CPU transforms with wide GPU mode disabled; ordinary props stay CPU\n'
    (build.BUILD/'graphics-host-validation.log').write_text(result.stdout+result.stderr)
    print(result.stdout)

if __name__=='__main__':main()
