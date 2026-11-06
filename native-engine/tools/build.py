#!/usr/bin/env python3
"""Cross-compile the actual decompiled engine; no recreation sources are used."""
import argparse,concurrent.futures,difflib,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];TARGET=ROOT/'native-engine';UP=ROOT/'upstream/goldeneye-pc';BUILD=TARGET/'build'
CC=Path('/opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc')
UPSTREAM_REV='fda76b69837e6c5aae436e0d3e798989e49b06d2'
# Keep original libc implementations for the game without overriding newlib's
# implementations used internally by libctru. This changes symbol names only.
GAME_LIBC=['strcpy','strncpy','strcat','strcmp','strncmp','toupper','isdigit','isalpha','isspace','strtol','sprintf']
DEFS=['PORT=1','PLATFORM_3DS=1','_LANGUAGE_C=1','AVOID_UB=1','PAL=0','VERSION_US','LANG_US','REFRESH_NTSC','LEFTOVERDEBUG','LEFTOVERSPECTRUM','BUGFIX_R0','BYTEMATCH','__3DS__','osGetTime=ge_osGetTime',*[f'{name}=ge_{name}' for name in GAME_LIBC]]
FLAGS=['-fmacro-prefix-map='+str(ROOT)+'=.','-std=gnu11','-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-O2','-fno-strict-aliasing','-funsigned-char','-fwrapv','-fno-stack-protector','-fno-short-enums','-fplan9-extensions','-fpermissive','-ffunction-sections','-fdata-sections','-w','-fmax-errors=4']
INCLUDES=[TARGET/'include',BUILD/'overlay',UP/'port/shim',UP,UP/'include',UP/'include/PR',UP/'src',UP/'src/game',UP/'src/libultra',UP/'src/libultra/audio',UP/'port/include',BUILD/'include',BUILD/'overlay',Path('/opt/devkitpro/libctru/include')]
def prepare():
    revision=subprocess.check_output(['git','-C',str(UP),'rev-parse','HEAD'],text=True).strip()
    if revision!=UPSTREAM_REV:raise ValueError('Upstream revision changed; refresh the native source/layout manifest before building.')
    (BUILD/'include').mkdir(parents=True,exist_ok=True);(BUILD/'logs').mkdir(exist_ok=True)
    import rom_data_plan
    rom_data_plan.emit_runtime_header()
    import title_asset_layout
    title_asset_layout.generate()
    import object_lod_assets
    object_lod_assets.generate()
    import bg_lod_assets
    bg_lod_assets.generate()
    import room_packets
    room_packets.generate()
    import room_lod_assets
    room_lod_assets.generate()
    std=Path('/opt/devkitpro/devkitARM/arm-none-eabi/include')
    for name in ('string','stdlib','sched','pthread'):
        (BUILD/'include'/f'host{name}.h').write_text(f'#include "{std/name}.h"\n')
    path=subprocess.check_output([str(CC),'-print-file-name=include/stddef.h'],text=True).strip()
    (BUILD/'include'/'hoststddef.h').write_text(f'#include "{path}"\n')
    (BUILD/'include'/'versioninfo.h').write_text('#define GE007_VERSION_HASH "fda76b6-3ds"\n#define GE007_VERSION "native-bringup"\n#define GE007_VERSION_CODENAME "3ds"\n#define GE007_ORIGIN_URL "https://github.com/jkdansereau/goldeneye-pc-port"\n#define GE007_TARGET_PLATFORM "armv6k-3ds"\n#define GE007_ROMID "ntsc-final"\n#define GE007_IS_PAL 0\n')
    system=BUILD/'include/libultra-system';system.mkdir(exist_ok=True)
    (system/'sched.h').write_text('#include "hostsched.h"\n')
    original=(UP/'port/src/romdata.c').read_text()
    start=original.index('static u32 romdataBswap32(u32 v)\n{')
    end=original.index('\n/*',original.index('void romdataFixupBriefing(u8 *blob)'))
    (BUILD/'overlay').mkdir(exist_ok=True)
    slots=(UP/'src/game/objecthandler.h').read_text()
    replacements={
        'char pad04[0x0C];':'char pad04[__builtin_offsetof(struct Model,obj) - 4];',
        'char pad14[0x0C];':'char pad14[__builtin_offsetof(struct Model,datas) - __builtin_offsetof(struct Model,obj) - 4];',
        'char pad28[sizeof(struct Model) - 0x28];':'char pad28[sizeof(struct Model) - __builtin_offsetof(struct Model,datas) - sizeof(void*)];',
    }
    for old,new in replacements.items():
        if slots.count(old)!=2:raise ValueError('Original model slot layout changed: '+old)
        slots=slots.replace(old,new)
    (BUILD/'overlay/objecthandler.h').write_text(slots)
    (BUILD/'overlay/rom_scalar_fixups.inc').write_text(original[start:end]+'\n')
    digest=hashlib.sha256(UPSTREAM_REV.encode())
    inputs=[TARGET/'source-files.json',TARGET/'rom-data-layout.json',TARGET/'rom-data-code-review.json',TARGET/'rom-string-layout.json',TARGET/'engine-state-review.json',TARGET/'rom-code-pool-review.json']
    for folder in ('src','include','tools','tests'):
        inputs += sorted(p for p in (TARGET/folder).rglob('*') if p.suffix in ('.c','.cpp','.h','.py','.pica'))
    inputs += sorted((TARGET/'vendor/GamepadMotionHelpers').glob('*'))
    inputs += sorted((TARGET/'vendor/rt64-stereo').glob('*'))
    for path in inputs:
        digest.update(str(path.relative_to(TARGET)).encode());digest.update(path.read_bytes())
    (BUILD/'include/native_build.h').write_text('#define NATIVE_BUILD_ID "'+digest.hexdigest()[:12]+'"\n')
def source_path(rel):
    source=UP/rel
    if str(rel)=='src/game/ramromreplay.c':
        text='#include "native_demo.h"\n'+source.read_text()
        old='''    if (ramrom_blkbuf_2->randseed != (u8)g_randomSeed)
    {
        ramromFadeToTitle();
    }'''
        if text.count(old)!=1:raise ValueError('Original replay seed check changed')
        text=text.replace(old,'    native_demo_replay_seed(ramrom_blkbuf_2->randseed,(u8)g_randomSeed);')
        for condition,reason in [('ramrom_blkbuf_2->check != var_t0','NATIVE_DEMO_CHECKSUM'),
                                 ('joyGetButtonsPressedThisFrame(0, 0xFFFFU) != 0','NATIVE_DEMO_INPUT'),
                                 ('var_a3 == 0 && ramrom_blkbuf_2->speedframes == 0','NATIVE_DEMO_END')]:
            old='    if ('+condition+')\n    {\n        ramromFadeToTitle();'
            if text.count(old)!=1:raise ValueError('Original replay exit changed: '+condition)
            text=text.replace(old,old.replace('        ramromFadeToTitle();','        native_demo_replay_stop('+reason+');\n        ramromFadeToTitle();'))
        old='    if ((g_GlobalTimer >= temp_v1) && ((g_GlobalTimer - g_ClockTimer) < temp_v1))\n    {\n        ramromFadeToTitle();'
        assert text.count(old)==1
        text=text.replace(old,old.replace('        ramromFadeToTitle();','        native_demo_replay_stop(NATIVE_DEMO_TIMER);\n        ramromFadeToTitle();'))
        old='void replay_recorded_ramrom_at_address(ramromfilestructure *demofile)\n{'
        assert text.count(old)==1
        text=text.replace(old,old+'\n    native_demo_replay_begin();')
        dest=BUILD/'overlay/ramromreplay.c';dest.write_text(text);return dest
    if str(rel)=='assets/rarewarelogo.c':
        text='#ifdef __3DS__\n#include "native_title_storage.h"\n#else\n'+source.read_text()+'\n#endif\n'
        dest=BUILD/'overlay/rarewarelogo.c';dest.write_text(text)
        return dest
    if str(rel)=='src/game/bondview_r.c':
        from subsistence import loadout
        text=loadout(source.read_text())
        dest=BUILD/'overlay/bondview_r.c';dest.write_text(text);return dest
    if str(rel)=='src/game/bondview2.c':
        text=source.read_text()
        replacements={
            '        return VIEWPORT_HEIGHT_DEFAULT;':'        return SCREEN_HEIGHT; /* Native LCD: no CRT overscan in Full mode. */',
            '            return VIEWPORT_ULY_DEFAULT;':'            return 0; /* Full uses all 240 native rows. */',
        }
        for old,new in replacements.items():
            if text.count(old)!=1:raise ValueError('Original Full viewport boundary changed: '+old)
            text=text.replace(old,new)
        text='#include "native_watch_environment.h"\n#include "native_controls.h"\n#include "native_demo.h"\n#include "native_theme.h"\n'+text
        old='    g_CurrentPlayer->field_D0 = 0;'
        if text.count(old)!=1:raise ValueError('Original movement hook boundary changed')
        text=text.replace(old,'    native_controls_apply(&moveData);\n'+old)
        # Scale only the two manual camera angle integrators. Multiplying the
        # stored speed feeds gain back into original aim acceleration each tick.
        for axis in ('verta','theta'):
            old='g_CurrentPlayer->speed'+axis+' * g_GlobalTimerDelta * 3.5f'
            assert text.count(old)==1,(axis,text.count(old))
            text=text.replace(old,'native_controls_look_rate(g_CurrentPlayer->speed'+axis+') * g_GlobalTimerDelta * 3.5f')
        anchor='    bondviewApplyVertaTheta();\n\n    // Handle crouching'
        assert text.count(anchor)==1
        text=text.replace(anchor,'    native_controls_apply_gyro();\n'+anchor)
        old='    return g_CurrentPlayer->bondbreathing;'
        assert text.count(old)==1
        text=text.replace(old,'    return !native_demo_active() && !native_setting_stamina_sway()?0.f:g_CurrentPlayer->bondbreathing;')
        old='    bheadUpdate(percent_speed, speedsideways);'
        assert text.count(old)==1
        text=text.replace(old,old+"\n    native_controls_scope_head();")
        signature='Gfx *bondviewRenderWatch(Gfx *gdl)'
        declarations='static void native_watch_skin_begin(ModelFileHeader *);\nstatic void native_watch_skin_end(void);\nstatic Gfx *native_watch_skin_draw(Gfx *,Mtxf *);\n'
        text=text.replace(signature,declarations+signature,1)
        old='        subdraw(&renderdata, (Model *) (&g_CurrentPlayer->something_with_watch_object_instance));'
        assert text.count(old)==1
        text=text.replace(old,'        native_watch_skin_begin(objheader);\n'+old+'\n        native_watch_skin_end();')
        old='        matrix_4x4_7F058C64();\n        matrix_4x4_f32_to_s32(&handmtx, finalmtx);'
        assert text.count(old)==1
        text=text.replace(old,'        gdl=native_watch_skin_draw(gdl,matrices);\n'+old)
        # The original fully-open branch applies an additional black shade.
        # Keep the environment's existing tile tint/alpha throughout the watch.
        a=text.index('        if ((g_CurrentPlayer->watch_animation_state == 5)',text.index('Gfx *bondviewRenderWatch('))
        b=text.index('        native_watch_skin_begin(objheader);',a)
        text=text[:a]+'''        renderdata.envcolour.word = native_watch_environment_rgba(g_CurrentPlayer->tileColor.r,g_CurrentPlayer->tileColor.g,g_CurrentPlayer->tileColor.b,g_CurrentPlayer->tileColor.a);

'''+text[b:]
        # The physical watch and orthographic HUD gauges are not world-camera
        # geometry. End the model scope before drawing the watch UI itself.
        a=text.index('Gfx *bondviewRenderWatch(Gfx *gdl)')
        b=text.index('Gfx *bondviewRenderGaugeBars(Gfx *gdl)',a)
        watch=text[a:b]
        old='        renderdata.gdl = gdl;'
        assert watch.count(old)==1
        watch=watch.replace(old,'        gDma0p(gdl++,0x3a,NATIVE_WATCH_BODY_SCOPE,0xffffffu);\n'+old)
        old='        gdl=native_watch_skin_draw(gdl,matrices);'
        assert watch.count(old)==1
        watch=watch.replace(old,old+'\n        gDma0p(gdl++,0x3a,0,(~NATIVE_WATCH_BODY_SCOPE)&0xffffffu);')
        text=text[:a]+watch+text[b:]
        a=text.index('Gfx *bondviewRenderGaugeBars(Gfx *gdl)')
        b=text.index('\nvoid mp_respawn_handler(',a)
        gauges=text[a:b]
        assert gauges.count('    return gdl;')==1
        gauges=gauges.replace('{\n','{\n    gDma0p(gdl++,0x3a,NATIVE_WATCH_BODY_SCOPE,0xffffffu);\n',1)
        gauges=gauges.replace('    return gdl;','    gDma0p(gdl++,0x3a,0,(~NATIVE_WATCH_BODY_SCOPE)&0xffffffu);\n    return gdl;')
        text=text[:a]+gauges+text[b:]
        text+='\n#include "native_watch_skin.h"\n'
        dest=BUILD/'overlay/bondview2.c';dest.parent.mkdir(exist_ok=True);dest.write_text(text)
        (dest.parent/'bondview2.patch').write_text(''.join(difflib.unified_diff(source.read_text().splitlines(True),text.splitlines(True),fromfile='upstream/src/game/bondview2.c',tofile='native/src/game/bondview2.c')))
        return dest
    if str(rel)=='src/game/options.c':
        text='#include <ultra64.h>\n#include "native_aspect.h"\n#include "native_settings.h"\n#include "native_theme.h"\nstatic Gfx *native_draw_control_styles(Gfx *);\nstatic Gfx *native_draw_controls_help(Gfx *);\nstatic Gfx *native_draw_colour_choice(Gfx *,int,int,unsigned);\nstatic Gfx *native_watch_theme_scope(Gfx *,int);\nstatic int native_watch_brief_scroll;\n'+source.read_text()
        text='#include <ultra64.h>\n#include "bondconstants.h"\nextern u8 *get_ptr_text_for_watch_breifing_page(WATCH_BRIEFING_PAGE);\nextern u8 *get_text_for_objective(int);\nextern s32 get_difficulty_for_objective(s32);\nextern u16 *bondinvGetFirstTitlebyIndex(s32);\nextern u16 *bondinvGetSecondTitlebyIndex(s32);\nstatic void native_bottom_watch_publish(void);\n'+text
        text='#include "native_objectives_capture.h"\n'+text
        # Collect the ORIGINAL briefing/objective layout in read-only mode
        # during gameplay. Normal open-watch rendering still calls through.
        a=text.index('Gfx *draw_watch_mission_briefing_page(Gfx *gdl, Mtx *param_2)\n{')
        b=text.index('\n//D:80058440',a)
        briefing=text[a:b]
        briefing=briefing.replace('    gdl = draw_background_health_and_armor(gdl, param_2, 0);','    if(!native_objective_is_collecting())gdl = draw_background_health_and_armor(gdl, param_2, 0);')
        briefing=briefing.replace('if (check_watch_page_transistion_running() != 1)','if (native_objective_is_collecting() || check_watch_page_transistion_running() != 1)')
        briefing=briefing.replace('gdl = microcode_constructor(gdl);','if(!native_objective_is_collecting())gdl = microcode_constructor(gdl);')
        briefing=briefing.replace('draw_blackbox_to_screen(', 'native_objective_blackbox(').replace('textRender(', 'native_objective_text(').replace('setTextOverlapCorrection(', 'native_objective_overlap(')
        briefing=briefing.replace('switch (mission_brief_index)', 'switch (native_objective_is_collecting()?BRIEF_INDEX_OBJECTIVES:mission_brief_index)')
        for name in ('mission_brief_background_navigation','mission_brief_m_briefing_navigation','mission_brief_q_branch_navigation','mission_brief_moneypenny_navigation','mission_brief_objectives_navigation'):
            briefing=briefing.replace(name+'();','if(!native_objective_is_collecting())'+name+'();')
        text=text[:a]+briefing+text[b:]
        for column in (1,2):
            old=f'langGet(drawentry->text[{column}])'
            if text.count(old)!=1:raise ValueError('Original watch ratio label changed')
            text=text.replace(old,f'(drawentry == &game_options_entries[PLAYER_OPTION_RATIO] ? (u8 *)native_aspect_label({column-1}) : {old})')
        # New styles occupy new records, leaving every original index intact.
        start=text.index('u16 game_control_styles[] = {');end=text.index('\n};',start)
        template=text[start:end].splitlines()[1].strip().rstrip(',')
        for n in range(2):
            row=template[template.index('getStringID'):]
            row=row.replace('getStringID(LOPTIONS, OPTION_STR_09_11HONEY_LF)',hex(0xf000+n),1)
            text=text[:end]+',\n    '+row+text[end:];end=text.index('\n};',end)
        start=text.index('struct game_options game_options_entries[] = {');end=text.index('\n};',start)
        comma=text.rfind('}',start,end)+1
        text=text[:comma]+','+text[comma:];end+=1
        text=text[:end]+'\n    {{0xf010,0,0,0},0},\n    {{0xf011,0,0,0},0},\n    {{0xf012,0,0,0},1},\n    {{0xf013,0,0,0},1}'+text[end:]
        old='  return g_CurrentPlayer->cur_player_control_type_0;'
        if text.count(old)!=1:raise ValueError('Original control-style getter changed')
        text=text.replace(old,'  int type=g_CurrentPlayer->cur_player_control_type_0;\n  return type>=8?CONTROLLER_CONFIG_SOLITARE:type;')
        old='void cur_player_set_control_type(int type)\n{'
        if text.count(old)!=1:raise ValueError('Original control-style setter changed')
        text=text.replace(old,old+'\n    int chosen=native_setting_style();\n    if(chosen>=0)type=chosen;')
        old='    g_CurrentPlayer->cur_player_control_type_1 = CONTROLLER_CONFIG_HONEY;\n    g_CurrentPlayer->cur_player_control_type_0 = CONTROLLER_CONFIG_HONEY;\n    g_CurrentPlayer->cur_player_control_type_2 = 0.0f;'
        if text.count(old)!=1:raise ValueError('Original watch control reset changed')
        text=text.replace(old,'    cur_player_set_control_type(CONTROLLER_CONFIG_HONEY);')
        start=text.index('Gfx *draw_controller_style_text(Gfx *gdl)');end=text.index('Gfx *sub_GAME_7F0A9AB8',start)
        text=text[:start]+'Gfx *draw_controller_style_text(Gfx *gdl) { return native_draw_control_styles(gdl); }\n\n'+text[end:]
        text=text.replace('Gfx *draw_watch_controller(Gfx *gdl)\n{','Gfx *draw_watch_controller(Gfx *gdl)\n{\n    if(g_CurrentPlayer->cur_player_control_type_0>=8)return native_draw_controls_help(gdl);')
        text=text.replace('s32 D_800409D8 = 8;','s32 D_800409D8 = 10;').replace('D_800409D8 = 4;','D_800409D8 = 6;').replace('D_800409D8 = 8;','D_800409D8 = 10;')
        # Four additional option rows after Ratio; music/FX keep their positions.
        text=text.replace('if (aux >= 10)','if (aux >= 14)')
        text=text.replace('game_options_index = GAME_OPTIONS_INDEX_RATIO;','game_options_index = 13;')
        text=text.replace('case GAME_OPTIONS_INDEX_RATIO:\n','case GAME_OPTIONS_INDEX_RATIO:\n                case 10:\n                case 11:\n                case 12:\n                case 13:\n')
        text=text.replace('i < 8; i = i + 1','i < 12; i = i + 1')
        text=text.replace('y_offset = y_offset + YINC','y_offset = y_offset + 11')
        old='void game_option_toggle_input(s32 option_index)\n{'
        new=old+\
            '\n    if(option_index>=8) {\n        if(!watch_item_is_actively_selected)return;\n        int value=option_index==8?native_setting_crosshair():option_index==9?native_setting_watch():option_index==10?native_setting_aa():native_setting_fps()==60;\n        int count=option_index==8?7:option_index==9?7:2;\n        int left=joyGetButtonsPressedThisFrame(PLAYER_1,L_CBUTTONS|L_TRIG|L_JPAD)||sub_GAME_7F0A4FB0();\n        int right=joyGetButtonsPressedThisFrame(PLAYER_1,R_CBUTTONS|R_TRIG|R_JPAD)||sub_GAME_7F0A4FEC();\n        if(left || right) {\n            value=(value+(left?-1:1)+count)%count;\n            game_option_select_value(&game_options_entries[option_index].current_value,value);\n            if(option_index==8)native_setting_set_crosshair(value);else if(option_index==9)native_setting_set_watch(value);else if(option_index==10)native_setting_set_aa(value);else native_setting_set_fps(value?60:30);\n        }\n        return;\n    }'
        text=text.replace(old,new)
        old='Gfx *draw_toggle_option_values(Gfx *gdl, s32 y, s32 option_index, u32 state)\n{'
        text=text.replace(old,old+'\n    if(option_index>=8)return native_draw_colour_choice(gdl,y,option_index,state);')
        text=text.replace('Gfx *draw_watch_game_options_page(Gfx *gdl, Mtx *param_2) {','Gfx *draw_watch_game_options_page(Gfx *gdl, Mtx *param_2) {\n    if(native_setting_aspect()>=0)game_options_entries[PLAYER_OPTION_RATIO].current_value=native_setting_aspect();')
        old='    game_options_entries[PLAYER_OPTION_RATIO].current_value = ratio_option;'
        text=text.replace(old,'    int saved=native_setting_aspect();\n    game_options_entries[PLAYER_OPTION_RATIO].current_value = saved>=0?saved:ratio_option;')
        old='    *param_1 = param_2;\n    set_controlstick_lr_disabled();'
        text=text.replace(old,'    *param_1 = param_2;\n    if(param_1==&game_options_entries[PLAYER_OPTION_RATIO].current_value)native_setting_set_aspect(param_2);\n    set_controlstick_lr_disabled();')
        start=text.index('Gfx *draw_watch_current_page(Gfx *gdl, Mtx *arg1, s32 watch_transitioning)')
        body=text[start:];body=body.replace('{\n    set_page_rectangle_colors','{\n    if(watch_transitioning)native_bottom_watch_publish();\n    gdl=native_watch_theme_scope(gdl,1);\n    set_page_rectangle_colors',1)
        body=body.replace('    return gdl;','    return native_watch_theme_scope(gdl,0);',1)
        text=text[:start]+body+'\n#include "native_watch_ui.h"\n'
        needle='    gSPDisplayList(gdl++, OS_PHYSICAL_TO_K0(g_CurrentPlayer->buffer_for_watch_greenbackdrop_DL));'
        assert text.count(needle)==1
        text=text.replace(needle,'    gDma0p(gdl++,0x3a,NATIVE_WATCH_TABS_SCOPE,0xffffffu);\n'+needle+'\n    gDma0p(gdl++,0x3a,0,(~NATIVE_WATCH_TABS_SCOPE)&0xffffffu);')
        # Scroll original briefing glyphs inside a fixed body window on both
        # displays. Read-only objective capture must not emit display commands.
        a=text.index('Gfx *draw_watch_mission_briefing_page(Gfx *gdl, Mtx *param_2)\n{')
        b=text.index('//D:80058440',a)
        briefing=text[a:b]
        old='        gdl = native_objective_text(gdl, &boxLeft, &boxTop, wrappedText, chars, font, 0x00ff00b0, viGetX(), viGetY(), 0, 10);'
        assert briefing.count(old)==1
        new='''        if(!native_objective_is_collecting()) {
            int bodyTop=boxTop,available=210-bodyTop;
            int maximum=textHeight>available?textHeight-available:0;
            if(native_watch_brief_scroll>maximum)native_watch_brief_scroll=maximum;
            boxTop-=native_watch_brief_scroll;
            gDPSetScissor(gdl++,G_SC_NON_INTERLACE,0,bodyTop,viGetX(),210);
        }
'''+old+'''
        if(!native_objective_is_collecting())gDPSetScissor(gdl++,G_SC_NON_INTERLACE,0,0,viGetX(),viGetY());'''
        briefing=briefing.replace(old,new)
        text=text[:a]+briefing+text[b:]
        signature='Gfx *draw_background_health_and_armor(Gfx *gdl, Mtx *arg1, s32 zoom_squish)'
        a=text.index(signature+'\n{');b=text.index('\nGfx *draw_background_health_and_armor_transitioning',a)
        background=text[a:b]
        background=background.replace('{\n','{\n    gDma0p(gdl++,0x3a,NATIVE_WATCH_BODY_SCOPE,0xffffffu);\n',1)
        assert background.count('    return gdl;')==1
        background=background.replace('    return gdl;','    gDma0p(gdl++,0x3a,0,(~NATIVE_WATCH_BODY_SCOPE)&0xffffffu);\n    return gdl;')
        text=text[:a]+background+text[b:]
        import native_options
        text=native_options.apply(text)
        import advanced_watch
        text=advanced_watch.apply(text)
        text+='\n#include "native_objectives_capture_impl.h"\n#include "native_bottom_watch.h"\n'
        text='#include <stdio.h>\n'+text
        dest=BUILD/'overlay/options.c';dest.parent.mkdir(exist_ok=True);dest.write_text(text)
        (dest.parent/'options.patch').write_text(''.join(difflib.unified_diff(source.read_text().splitlines(True),text.splitlines(True),fromfile='upstream/src/game/options.c',tofile='native/src/game/options.c')))
        return dest
    if str(rel)=='src/game/file2.c':
        text='#include "native_settings.h"\n'+source.read_text()
        assert text.count('if (portAllUnlocked &&')==1
        text=text.replace('if (portAllUnlocked &&','if (native_setting_unlock_all() &&')
        needle='s32 fileIs007ModeUnlocked(u32 folder)\n{'
        assert text.count(needle)==1
        text=text.replace(needle,needle+'\n    if(native_setting_unlock_all())return TRUE;')
        dest=BUILD/'overlay/file2.c';dest.write_text(text);return dest
    if str(rel)=='src/game/language.c':
        text='#include "native_settings.h"\n'+source.read_text()
        old='u8 * langGet(s32 slotID)\n{'
        if text.count(old)!=1:raise ValueError('Original language lookup changed')
        text=text.replace(old,old+'\n    const char *native=native_string((unsigned)slotID);\n    if(native)return (u8 *)native;')
        dest=BUILD/'overlay/language.c';dest.write_text(text);return dest
    if str(rel)=='src/game/bondview.c':
        text='#include "native_controls.h"\n'+source.read_text()
        old='cur_player_set_control_type(get_player_control_style(get_cur_playernum()));'
        if text.count(old)!=1:raise ValueError('Original control restore changed')
        text=text.replace(old,'cur_player_set_control_type(native_controls_restore(get_player_control_style(get_cur_playernum())));')
        dest=BUILD/'overlay/bondview.c';dest.write_text(text);return dest
    if str(rel)=='src/game/chr.c':
        text=source.read_text()
        # Render-only: never alter the shared on-screen verdict used by AI.
        at=text.index('Gfx *chrRenderProp(PropRecord *prop, Gfx *gdl, s32 withalpha)')
        brace=text.index('{',at)+1
        text=text[:brace]+'\n    if(!native_guard_activity_drawable(prop))return gdl;\n    if(!native_prop_detail_draw(prop,prop->chr && prop->chr->model?getinstsize(prop->chr->model):0.f,"guards"))return gdl;'+text[brace:]
        # Prop/model types are defined by the original includes, so include
        # the helper immediately before the rendering entry, not file top.
        at=text.index('Gfx *chrRenderProp(PropRecord *prop, Gfx *gdl, s32 withalpha)')
        text=text[:at]+'#include "native_prop_detail.h"\n'+text[at:]
        text='#include "native_guard_activity.h"\n#include "native_baked_lod.h"\n#include "native_scenery_scope.h"\n'+text
        old='            drawjointlist(&mrData, chr->field_20);'
        assert text.count(old)==1
        text=text.replace(old,'            int native_actor_faded=withalpha && spB8==3 && chrfadealpha>0;\n            int native_actor_batch=(!withalpha && chrfadealpha==255 && spB8==1) || native_actor_faded;\n            unsigned native_saved_tier=0;\n            if(native_actor_batch && &native_object_lod_tier){\n                coord3d *eye=bondviewGetCurrentPlayersPosition();\n                float dx=eye?prop->pos.x-eye->x:0,dy=eye?prop->pos.y-eye->y:0,dz=eye?prop->pos.z-eye->z:0;\n                native_saved_tier=native_object_lod_tier;\n                native_object_lod_tier=native_detail_tier(sqrtf(dx*dx+dy*dy+dz*dz)-getinstsize(chr->model),native_setting_lod_strength());\n            }\n            if(native_actor_batch){if(native_actor_faded){native_faded_character_scope(mrData.gdl++,1);}else {native_character_scope(mrData.gdl++,1);}}\n'+old+'\n            if(native_actor_batch)native_character_scope(mrData.gdl++,0);\n            if(native_actor_batch && &native_object_lod_tier)native_object_lod_tier=native_saved_tier;')
        import guard_pose
        text=guard_pose.profile(text)
        old='    s32 result=native_chr_tick_impl(prop);'
        assert text.count(old)==1
        text=text.replace(old,'''    s32 result;
    if(prop->type==PROP_TYPE_CHR && prop->chr && !native_guard_activity_awake(prop->chr)) {
        ChrRecord *chr=prop->chr;
        native_guard_activity_sleep(chr);
        if(chr->field_20){sub_GAME_7F06B248(chr->field_20);chr->field_20=NULL;}
        prop->flags&=~PROPFLAG_ONSCREEN;
        portPropSetGameplayOnScreen(prop,0);
        if(chr->weapons_held[GUNRIGHT])chr->weapons_held[GUNRIGHT]->flags&=~PROPFLAG_ONSCREEN;
        if(chr->weapons_held[GUNLEFT])chr->weapons_held[GUNLEFT]->flags&=~PROPFLAG_ONSCREEN;
        if(chr->handle_positiondata_hat)chr->handle_positiondata_hat->flags&=~PROPFLAG_ONSCREEN;
        result=TICKOP_NONE;
    } else result=native_chr_tick_impl(prop);''')
        dest=BUILD/'overlay/chr.c';dest.write_text(text);return dest
    if str(rel)=='src/game/chraction.c':
        text='#include "native_randomizer.h"\n#include "native_guard_activity.h"\n'+source.read_text()
        old='        sp38 = retrieve_header_for_body_and_head(bodyid, headid, (u32) arg1->bitflags);'
        if text.count(old)!=1:raise ValueError('Original guard appearance constructor changed')
        text=text.replace(old,'        s32 visualbody=bodyid,visualhead=headid;\n        native_randomizer_story_appearance(arg1->chrnum,&visualbody,&visualhead);\n        sp38 = retrieve_header_for_body_and_head(visualbody, visualhead, (u32) arg1->bitflags);')
        old='void chrlvActionTick(ChrRecord *self)\n{'
        assert text.count(old)==1
        text=text.replace(old,'#include "native_guard_activity_impl.h"\n\n'+old+
            '\n    if(!native_guard_activity_awake(self)){native_guard_activity_sleep(self);return;}')
        old='            ai(self, PROP_TYPE_CHR);'
        assert text.count(old)==1
        text=text.replace(old,'            if(native_guard_activity_ai_due(self))ai(self, PROP_TYPE_CHR);')
        for signature in ('bool chrCheckTargetInSight(ChrRecord *self)', 's32 chrIsTargetNearlyInSight(ChrRecord *self)'):
            needle=signature+'\n{'
            assert text.count(needle)==1
            text=text.replace(needle,needle+'\n    if(!native_guard_activity_sight_allowed(self))return FALSE;')
        dest=BUILD/'overlay/chraction.c';dest.write_text(text);return dest
    if str(rel)=='src/game/propobj.c':
        text='#include "native_guard_activity.h"\n#include "native_pickup.h"\n#include "native_settings.h"\n'+source.read_text()
        indexed='save_ptr_monitor_ani_code_to_obj_ani_slot(screen, (u32 *) _PORT_monAnimPtrs[m->time]);'
        assert text.count(indexed)==2
        text=text.replace(indexed,'\n#ifdef __3DS__\n                save_ptr_monitor_ani_code_to_obj_ani_slot(screen, (u32 *)(uintptr_t)m->time);\n#else\n                '+indexed+'\n#endif\n')
        old='(bondviewGetPlayerPitchRadians() < -0.7853982f) && (g_CurrentPlayer->magnetattracttime < 0)'
        if text.count(old)!=1:raise ValueError('Original automatic pickup look gate changed')
        text=text.replace(old,'!native_pickup_look_allowed(bondviewGetPlayerPitchRadians(),g_CurrentPlayer->magnetattracttime,native_setting_style())')
        text=text.replace('Gfx *chrobjRenderProp(PropRecord *prop, Gfx *gdl, s32 arg2)', '#include "native_prop_detail.h"\nGfx *chrobjRenderProp(PropRecord *prop, Gfx *gdl, s32 arg2)',1)
        render='Gfx *chrobjRenderProp(PropRecord *prop, Gfx *gdl, s32 arg2)\n{'
        assert text.count(render)==1
        # Guard subtree culling is inserted with the prop detail gate below.
        text='#include "native_scenery_scope.h"\n'+text
        marker='void sub_GAME_7F04AC20(PropRecord *prop, ModelRenderData *mrData, s32 arg2)\n{'
        assert text.count(marker)==1
        text=text.replace(marker,'#include "native_object_batch.h"\n#include "native_baked_lod.h"\n#include "native_detail.h"\n'+marker)
        text='#include "native_glass.h"\n'+text
        old='        subdraw(mrData, model);'
        assert text.count(old)==1
        prefix='''        int native_pane=obj->type==PROPDEF_GLASS || obj->type==PROPDEF_TINTED_GLASS;
        int native_object=native_object_batch_eligible(obj->type,arg2,destroyed);
        int native_distance_lod=!destroyed && (native_object || native_pane || obj->type==PROPDEF_DOOR);
        unsigned native_saved_tier=0,native_render_tier=0;
        if(native_distance_lod && &native_object_lod_tier){
            coord3d *eye=bondviewGetCurrentPlayersPosition();
            float dx=eye?prop->pos.x-eye->x:0,dy=eye?prop->pos.y-eye->y:0,dz=eye?prop->pos.z-eye->z:0;
            native_saved_tier=native_object_lod_tier;
            native_render_tier=native_detail_tier(sqrtf(dx*dx+dy*dy+dz*dz)-getinstsize(model),native_setting_lod_strength());
            native_object_lod_tier=native_render_tier;
        }
        int native_simple_pane=native_pane && native_glass_lod_simple(native_setting_glass(),native_render_tier);
        if(native_pane){
            native_glass_scope(mrData->gdl++,1);
            native_glass_detail_command(mrData->gdl++,native_simple_pane,native_render_tier);
        }
        if(native_simple_pane && !destroyed){native_simple_glass_scope(mrData->gdl++,1);}
        else if(native_object){if(obj->type==PROPDEF_VEHICHLE){native_vehicle_scope(mrData->gdl++,1);}else {native_object_scope(mrData->gdl++,1);}}
'''
        suffix='''
        if(native_simple_pane && !destroyed){native_simple_glass_scope(mrData->gdl++,0);}
        else if(native_object){if(obj->type==PROPDEF_VEHICHLE){native_vehicle_scope(mrData->gdl++,0);}else {native_object_scope(mrData->gdl++,0);}}
        if(native_pane)native_glass_scope(mrData->gdl++,0);
        if(native_distance_lod && &native_object_lod_tier)native_object_lod_tier=native_saved_tier;'''
        text=text.replace(old,prefix+old+suffix)
        signature='Gfx *chrobjRenderProp(PropRecord *prop, Gfx *gdl, s32 arg2)\n{'
        assert text.count(signature)==1
        text=text.replace(signature,signature+'\n    if(!native_guard_activity_drawable(prop))return gdl;\n    if(!native_prop_detail_draw(prop,prop->obj && prop->obj->model?getinstsize(prop->obj->model):0.f,"props"))return gdl;')
        a=text.index('f32 chrobjFogVisRangeRelated(PropRecord *prop, f32 size)');b=text.index('bool sub_GAME_7F054C58(',a)
        body=text[a:b];assert body.count('    return ret;')==1
        body=body.replace('    return ret;','    return ret*native_prop_detail_fade(prop,size);')
        text=text[:a]+body+text[b:]
        dest=BUILD/'overlay/propobj.c';dest.write_text(text);return dest
    if str(rel) in ('src/game/explosion.c','src/game/glass2.c'):
        import effect_lod
        text=effect_lod.explosion(source.read_text()) if rel.name=='explosion.c' else effect_lod.glass(source.read_text())
        dest=BUILD/'overlay'/rel.name;dest.write_text(text);return dest
    if str(rel)=='src/game/gunfire.c':
        text='#include "native_viewmodel.h"\n'+source.read_text()
        a=text.index('void gunRenderFirstPersonGunModels(Gfx **gdlptr)')
        b=text.index('Gfx *set_enviro_fog_for_items_in_solo_watch_menu',a)
        body=text[a:b]
        old='    Gfx *gdl = *gdlptr;'
        if body.count(old)!=1:raise ValueError('Original viewmodel pass entry changed')
        body=body.replace(old,old+'\n    native_viewmodel_scope(gdl++,1);')
        old='    *gdlptr = gdl;'
        if body.count(old)!=1:raise ValueError('Original viewmodel pass exit changed')
        body=body.replace(old,'    native_viewmodel_scope(gdl++,0);\n'+old)
        text=text[:a]+body+text[b:]
        import effect_lod
        text=effect_lod.gun(text)
        import dynamic_stereo
        text=dynamic_stereo.gun(text)
        dest=BUILD/'overlay/gunfire.c';dest.write_text(text);return dest
    if str(rel)=='src/game/bg.c':
        text=source.read_text()
        old='    end = (u8 *)&g_BgCurrentRoom;'
        if text.count(old)!=1:raise ValueError('Original special-portal extent boundary changed')
        text=text.replace(old,'    end = (u8 *)specialportalarray + sizeof(specialportalarray); /* Native globals are not adjacent. */')

        text='#include "native_room_render.h"\n'+text
        old_reset='    levelentry_index = 0;'
        assert text.count(old_reset)==1
        text=text.replace(old_reset,old_reset+'\n    native_room_render_reset();')
        old_loaded='    g_BgRoomInfo[roomID].model_bin_loaded = 1;'
        assert text.count(old_loaded)==1
        text=text.replace(old_loaded,old_loaded+'\n    native_room_render_stats.loaded++;')
        delete_start='    s_room_info *room = &g_BgRoomInfo[roomID];'
        assert text.count(delete_start)==1
        text=text.replace(delete_start,delete_start+'\n    if(room->model_bin_loaded)native_room_render_stats.unloaded++;')
        old='s32 bgViewRelated[] = { 1, 1, -1, -1 };'
        if text.count(old)!=1:raise ValueError('Original BG viewport inset changed')
        text=text.replace(old,'s32 bgViewRelated[] = { 0, 0, 0, 0 }; /* Native LCD: room bounds reach the viewport edges. */')
        at=text.index('Gfx *bgRenderRoomPrimary(Gfx *gdl, s32 room_index)\n{')
        text=text[:at]+'#include "native_distance_room.h"\n'+text[at:]
        for name in ('Primary','Secondary'):
            signature=f'Gfx *bgRenderRoom{name}(Gfx *gdl, s32 room_index)\n{{'
            assert text.count(signature)==1
            text=text.replace(signature,signature+'\n    if(!native_distance_room_visible(room_index))return gdl;')
        a=text.index('Gfx *bgRenderRoomPrimary(Gfx *gdl, s32 room_index)\n{')
        b=text.index('Gfx *bgRenderRoomSecondary(Gfx *gdl, s32 room_index)\n{',a)
        body=text[a:b]
        body=body.replace('if(!native_distance_room_visible(room_index))return gdl;','if(!native_distance_room_visible(room_index)){native_room_render_stats.distance_culled++;return gdl;}')
        old_budget='''            if (g_RoomLoadBudget > 0)
            {
                g_RoomLoadBudget--;
                bgLoadRoomModelData(room_index);
            }'''
        assert body.count(old_budget)==1
        body=body.replace(old_budget,'''            /* This room is already on the visible draw list. A three-room
             * streaming cap must not leave the remaining rooms as sky holes. */
            if(g_RoomLoadBudget>0)g_RoomLoadBudget--;
            else native_room_render_stats.budget_bypass++;
            bgLoadRoomModelData(room_index);''')
        missing='''        if (g_BgRoomInfo[room_index].model_bin_loaded == 0)
        {
#if defined(PORT)'''
        assert body.count(missing)==1
        body=body.replace(missing,missing.replace('#if defined(PORT)','            native_room_render_stats.missing++;\n#if defined(PORT)'))
        drawn='            gdl = applyRoomMatrixToDisplayList(gdl, room_index);'
        assert body.count(drawn)==1
        body=body.replace(drawn,'            native_room_render_stats.drawn++;\n'+drawn)
        text=text[:a]+body+text[b:]
        at=text.index('Gfx *bgLevelRender(Gfx *arg0)\n{')
        text=text[:at]+'#include "native_distance_fog_emit.h"\n'+text[at:]
        text=text.replace('Gfx *bgLevelRender(Gfx *arg0)\n{','Gfx *bgLevelRender(Gfx *arg0)\n{\n    native_room_render_sample();\n    arg0=native_distance_fog_begin(arg0);')
        text=text.replace('    return bondviewGfxPlayerField5cMatrix(arg0++);','    arg0=bondviewGfxPlayerField5cMatrix(arg0++);\n    return native_distance_fog_end(arg0);')
        text='#include "native_baked_lod.h"\n#include "native_scenery_scope.h"\n'+text
        a=text.index('Gfx *bgRenderRoomPrimary(Gfx *gdl, s32 room_index)\n{');b=text.index('Gfx *bgRenderRoomSecondary(Gfx *gdl, s32 room_index)\n{',a)
        body=text[a:b]
        old='            gSPDisplayList(gdl++, OS_K0_TO_PHYSICAL(g_BgRoomInfo[room_index].ptr_expanded_mapping_info));'
        assert body.count(old)==1
        body=body.replace(old,'            native_scenery_scope(gdl++,1);\n            Gfx *native_before=gdl;\n            if(native_room_lod_begin)gdl=native_room_lod_begin(gdl,room_index,g_BgRoomInfo[room_index].vertices,g_BgRoomInfo[room_index].usize_point_index_binary/16,native_room_detail_tier(room_index));\n            int native_lod_on=gdl!=native_before;\n'+old+'\n            if(native_lod_on)native_baked_lod_command(gdl++,(struct NativeBakedLodPacket *)NULL);\n            native_scenery_scope(gdl++,0);')
        text=text[:a]+body+text[b:]
        text='#include "native_room_packets.h"\n'+text
        loaded='    bgBuildRoomVtxBounds(roomID);'
        assert text.count(loaded)==1
        text=text.replace(loaded,'    if(native_room_packets_loaded)native_room_packets_loaded(roomID);\n'+loaded)
        text=text.replace(old,'            if(native_room_packets_begin)gdl=native_room_packets_begin(gdl,room_index,g_BgRoomInfo[room_index].ptr_expanded_mapping_info,g_BgRoomInfo[room_index].usize_primary_DL_binary);\n'+old+'\n            gDma0p(gdl++,NATIVE_ROOM_PACKET_COMMAND,0,0);')
        dest=BUILD/'overlay/bg.c' ;dest.write_text(text);return dest
    if str(rel)=='src/game/loadobjectmodel.c':
        text=source.read_text()
        old='s32 sizepropdef(PropDefHeaderRecord *pdef)\n{\n#ifdef PORT'
        if text.count(old)!=1:raise ValueError('Original setup stride boundary changed')
        text=text.replace(old,'s32 sizepropdef(PropDefHeaderRecord *pdef)\n{\n#if defined(PORT) && !defined(PLATFORM_3DS) /* Desktop sidecars use widened records; native ARM sidecars retain N64 strides. */')
        old='case PROPDEF_OBJECTIVE_COPY_ITEM:\n            return 1;//return sizeof(GlassRecord) / 4;'
        if text.count(old)!=1:raise ValueError('Original copy-objective stride boundary changed')
        text=text.replace(old,'case PROPDEF_OBJECTIVE_COPY_ITEM:\n            return 3; /* Retail setup record has three words, including its two scalar slots. */')
        # Native setup headers preserve state/type byte positions. Casting the
        # entire first word to u8 reads extrascale on little-endian ARM, including
        # at the END marker, and lets reuse searches run beyond the prop stream.
        old='(u8)cmd[0]'
        if text.count(old)!=2:raise ValueError('Original prop reuse header checks changed')
        text=text.replace(old,'((PropDefHeaderRecord *)cmd)->type')
        dest=BUILD/'overlay/loadobjectmodel.c';dest.parent.mkdir(exist_ok=True);dest.write_text(text)
        (dest.parent/'loadobjectmodel.patch').write_text(''.join(difflib.unified_diff(source.read_text().splitlines(True),text.splitlines(True),fromfile='upstream/src/game/loadobjectmodel.c',tofile='native/src/game/loadobjectmodel.c')))
        return dest
    if str(rel)=='src/game/prop.c':
        text=source.read_text()
        text='#include "native_randomizer.h"\n'+text
        text=text.replace('        // PD rejoins here','        native_randomizer_setup(stageId);\n\n        // PD rejoins here',1)
        start=text.index('void proplvreset2(enum LEVELID stageId)')
        body=text[start:]
        checkpoints=[
            ('    for (pitem = PitemZ_entries;', 'reset prop model headers'),
            ('        g_ptrStageSetupFile = _fileNameLoadToBank', 'load setup file'),
            ('        langLoadToAddr(', 'load stage language'),
            ('        g_CurrentSetup.pathwaypoints =', 'rebase setup header'),
            ('        if (g_CurrentSetup.pathwaypoints)', 'rebase waypoints'),
            ('        if (g_CurrentSetup.waypointgroups)', 'rebase waypoint groups'),
            ('        // Convert ailist pointers', 'rebase AI lists'),
            ('        if (g_CurrentSetup.patrolpaths)', 'rebase patrol paths'),
            ('        if (g_CurrentSetup.pads)', 'resolve pads'),
            ('        if (g_CurrentSetup.boundpads)', 'resolve bound pads'),
            ('        if (g_CurrentSetup.padnames)', 'rebase pad names'),
            ('        if (g_CurrentSetup.boundpadnames)', 'rebase bound pad names'),
            ('        if (withchrs)', 'count and allocate guards'),
            ('        if (withobjs)', 'count props'),
            ('        modelmgrAllocateModelSlots(numObjects);', 'allocate model slots'),
            ('        modelmgrAllocateAnimModelSlots(numAnimatedObjects);', 'allocate animated model slots'),
        ]
        for needle,label in checkpoints:
            pos=body.find(needle)
            if pos<0:raise ValueError('Original prop checkpoint changed: '+label)
            # Header assignment has both PORT and N64 branches: log once in PORT.
            body=body[:pos]+'        osSyncPrintf("Original props: '+label+'\\n");\n'+body[pos:]
        old='            while ((i1 = phead->type) != PROPDEF_END)\n            {'
        if body.count(old)!=1:raise ValueError('Original prop record loop changed')
        body=body.replace(old,old+'\n                osSyncPrintf("Original props: record %d type %d\\n",pdefIndex,(int)phead->type);')
        text=text[:start]+body
        dest=BUILD/'overlay/prop.c';dest.parent.mkdir(exist_ok=True);dest.write_text(text)
        return dest
    if str(rel) in ('src/game/objecthandler.c','src/game/initunk_005450.c'):
        text=source.read_text()
        old='#if defined(__x86_64__)'
        if text.count(old)!=1:raise ValueError('Original model-hit pool boundary changed')
        text=text.replace(old,'#if defined(PORT) /* Native linkers do not preserve the N64 placeholder ordering. */')
        dest=BUILD/'overlay'/source.name;dest.parent.mkdir(exist_ok=True);dest.write_text(text)
        (dest.parent/(source.stem+'.patch')).write_text(''.join(difflib.unified_diff(source.read_text().splitlines(True),text.splitlines(True),fromfile='upstream/'+str(rel),tofile='native/'+str(rel))))
        return dest
    if str(rel)=='src/game/image_bank.c':
        text=source.read_text()
        old='#if defined(__x86_64__)'
        if text.count(old)!=1:raise ValueError('Original global texture symbol boundary changed')
        text=text.replace(old,'#if defined(PORT) /* All relocated native executables need segment offsets, including ARM32. */')
        dest=BUILD/'overlay/image_bank.c';dest.parent.mkdir(exist_ok=True);dest.write_text(text)
        (dest.parent/'image_bank.patch').write_text(''.join(difflib.unified_diff(source.read_text().splitlines(True),text.splitlines(True),fromfile='upstream/src/game/image_bank.c',tofile='native/src/game/image_bank.c')))
        return dest
    if str(rel)=='src/game/front.c':
        text='#include "system.h"\n#include "native_settings.h"\n#include <stdio.h>\nstatic int native_front_update(void);\nstatic int native_front_is_open(void);\nstatic int native_front_options_blocked(void);\nstatic Gfx *native_front_draw(Gfx *);\nstatic int native_mode_y(int);\nstatic int native_mode_row(void);\nstatic int native_mode_extra(void);\nstatic void native_mode_activate(void);\nstatic Gfx *native_mode_draw_entries(Gfx *);\n'+source.read_text()
        from subsistence import frontend
        text=frontend(text)
        # Remaining title render counters also need elapsed VI time.
        for old,new in (('ninLogoRotRate += 0.0209439527243f;','ninLogoRotRate += 0.0209439527243f*g_ClockTimer;'),('ninLogoRotRate += 0.017453292f;','ninLogoRotRate += 0.017453292f*g_ClockTimer;'),('g_MenuTimer++;','g_MenuTimer += g_ClockTimer;')):
            assert text.count(old)==1,old
            text=text.replace(old,new)

        text='static float native_bottom_folder_x[4],native_bottom_folder_y[4];\nstatic void native_bottom_front_events(void);\nstatic void native_bottom_front_publish(void);\n'+text
        needle='            projectRectCornersTo2D(&folderpositions_camspace[foldernum], &xminmax, &yminmax, &folderbbox.right, &folderbbox.left);'
        assert text.count(needle)==1
        text=text.replace(needle,needle+'\n            native_bottom_folder_x[foldernum]=(folderbbox.right+folderbbox.left)*.5f;\n            native_bottom_folder_y[foldernum]=(folderbbox.up+folderbbox.down)*.5f;')
        start=text.index('s32 frontCheckIfCheatIsUnlocked(s32 cheat)')
        end=text.index('\n#ifdef PORT',start)
        body=text[start:end]
        body=body.replace('return fileGetIsCheatUnlocked(', 'return native_setting_unlock_all() || fileGetIsCheatUnlocked(')
        for query in ('fileIsCradleCompletedForFolder','fileIsAztecCompletedOnSecretOr00ForFolder','fileIsEgyptCompletedOn00ForFolder'):
            body=body.replace('return '+query+'(', 'return native_setting_unlock_all() || '+query+'(')
        text=text[:start]+body+text[end:]
        start=text.index('void interface_menu06_modesel(void)');end=text.index('// Address 0x7F00D5E8',start)
        body=text[start:end]
        needle='    else if ((is_cheat_menu_available) && (275.0f <= cursor_v_pos))'
        assert body.count(needle)==1
        body=body.replace(needle,'    else if(native_mode_extra()>=0) {\n        mission_difficulty_highlighted=DIFFICULTY_MULTI;\n        if(joyGetButtonsPressedThisFrame(PLAYER_1,START_BUTTON|Z_TRIG|A_BUTTON))native_mode_activate();\n    }\n    else if ((is_cheat_menu_available) && native_mode_row()==2)')
        body=body.replace('(243.0f <= cursor_v_pos)','(native_mode_row()==1)')
        text=text[:start]+body+text[end:]
        needle='    cursor_v_pos = mode * 0x20 + 0xe2;'
        assert text.count(needle)==1
        text=text.replace(needle,'    cursor_v_pos = native_mode_y(mode)+6;')
        start=text.index('Gfx* constructor_menu06_modesel(Gfx* DL)');end=text.index('\ns32 pull_and_display_text',start)
        body=text[start:end]
        for row,base in enumerate((0xdc,0xfc,0x11c)):
            body,n=re.subn(r'y = 0x'+format(base,'x')+r';',f'y = native_mode_y({row});',body,flags=re.I)
            assert n==2
            body,n=re.subn(r'0x94, 0x'+format(base-2,'x')+r', y2 \+ 0xAF, 0x'+format(base+14,'x'),f'0x94, native_mode_y({row})-2, y2 + 0xAF, native_mode_y({row})+14',body,flags=re.I)
            assert n==1
        needle='    DL = frontAddPreviousTabText(DL);'
        assert body.count(needle)==1
        body=body.replace(needle,'    DL = native_mode_draw_entries(DL);\n'+needle)
        text=text[:start]+body+text[end:]
        start=text.index('void menu_init(void)');end=text.index('Gfx * menu_jump_constructor_handler',start)
        body=text[start:end];needle='\n    switch(current_menu) {'
        assert body.count(needle)==1
        body=body.replace(needle,'\n    if(native_front_update())return;'+needle)
        text=text[:start]+body+text[end:]
        start=text.index('Gfx * menu_jump_constructor_handler');end=text.index('\n}',start)+2
        body=text[start:end];body=body.replace('{','{\n    if(native_front_is_open()){native_bottom_front_publish();return native_front_draw(DL);}',1)
        pos=body.rindex('    return DL;');body=body[:pos]+body[pos:].replace('    return DL;','    native_bottom_front_publish();\n    return native_front_draw(DL);',1)
        text=text[:start]+body+text[end:]
        needle='    display_image_at_position(&DL, &xypos, &halfedxy, image->width, image->height, 0, 0, 1, 255, 255, 255, 220, (image->level > 0), 0);'
        assert text.count(needle)==1
        text=text.replace(needle,'    s32 r=255,g=255,b=255;\n    extern void portCrosshairTint(s32 *,s32 *,s32 *);\n    extern void portCrosshairApplyTintCombine(Gfx *);\n    if(option==0)portCrosshairTint(&r,&g,&b);\n    Gfx *env=DL;\n'+needle.replace('255, 255, 255, 220','r, g, b, 220')+'\n    if(option==0)portCrosshairApplyTintCombine(env);')
        # All retail demo stages are converted now. Keep the original attract
        # transition, which reads recorded input packets from the user's ROM.
        if text.count('            select_ramrom_to_play();')!=1:
            raise ValueError('Original attract-demo transition changed')
        text=text.replace('PORT_HUD_ASPECT(DL, GE_HUD_ASPECT_CENTER);','PORT_HUD_ASPECT(DL, GE_HUD_ASPECT_NONE);')
        text=re.sub(r'viSetAspect\(([^;\n]*)\);',r'viSetAspect(native_frontend_aspect(\1));',text)
        text='''#include <ultra64.h>
#include "bondconstants.h"
extern f32 portNativeAspect(void);
extern MENU get_currentmenu(void);
/* Folder meshes and their 2D text share the authored 4:3 canvas. The
 * renderer maps that complete canvas to the selected physical output. */
static f32 native_frontend_aspect(f32 authored) {
    int menu=get_currentmenu();
    if((menu>=MENU_FILE_SELECT && menu<=MENU_BRIEFING) ||
       (menu>=MENU_MISSION_FAILED && menu<=MENU_NO_CONTROLLERS))return authored;
    f32 output=portNativeAspect();return output>0?output:authored;
}
'''+text
        import multiplayer_preview
        text=multiplayer_preview.front(text)
        text+='\n#include "native_front_menu.h"\n#include "native_bottom_front.h"\n'
        dest=BUILD/'overlay/front.c';dest.write_text(text)
        return dest
    if str(rel)=='src/game/title2.c':
        text=source.read_text();start=text.index('Gfx* titleRenderFolderMenuBackgroundLines(');end=text.index('\n\n/**',start)
        body=text[start:end]
        a=body.index('        gDPLoadTextureBlock(');z=body.index('        );',a)+len('        );')
        body=body[:a]+body[z:]
        old='    i = 0;'
        body=body.replace(old,'    /* Native Fast3D has no 4KB TMEM limit. Load the complete original I8\n     * image once, retaining all original row geometry and gradient values. */\n    gDPLoadTextureTile(gdl++,image,G_IM_FMT_I,G_IM_SIZ_8b,440,299,\n        0,0,439,298,0,G_TX_CLAMP,G_TX_CLAMP,0,0,0,0);\n    i = 0;')
        body=body.replace('(-xOffset) << 5, 0,','(-xOffset) << 5, i << 5,')
        body=body.replace('G_TX_RENDERTILE, 0, 0,','G_TX_RENDERTILE, 0, i << 5,')
        text=text[:start]+body+text[end:]
        dest=BUILD/'overlay/title2.c';dest.write_text(text)
        return dest
    if str(rel)=='src/snd.c':
        text=source.read_text()
        old='sound = (soundBank->instArray[0]->soundArray[soundIndex - 1]);'
        if text.count(old)!=1:raise ValueError('Original sound index boundary changed')
        text=text.replace(old,'sound = (soundBank->instArray[0]->soundArray[soundIndex - (__SIZEOF_POINTER__ == 8 ? 1 : 0)]);')
        dest=BUILD/'overlay/snd.c';dest.write_text(text)
        return dest
    if str(rel)=='src/game/zlib.c':
        text='#include "native_inflate.h"\n#include "system.h"\n'+source.read_text()
        old='\t\t\t\tq = &rz_hlist[rz_hufts];'
        if text.count(old)!=1:raise ValueError('Original Huffman table allocation changed')
        text=text.replace(old,'\t\t\t\tif (rz_hufts > NATIVE_HUFT_ENTRIES || z + 1 > NATIVE_HUFT_ENTRIES - rz_hufts)\n\t\t\t\t\tsysFatalError("Original inflate Huffman workspace exhausted (%u + %u)",rz_hufts,z+1);\n'+old)
        dest=BUILD/'overlay/zlib.c';dest.write_text(text)
        return dest
    if str(rel)=='src/libultra/audio/synthesizer.c':
        import dsp_audio
        text=dsp_audio.synth(source.read_text())
        dest=BUILD/'overlay/synthesizer.c';dest.write_text(text);return dest
    if str(rel)=='port/src/mixer.c':
        text=source.read_text()
        text='#include "native_audio_pcm.h"\n'+text
        text=text.replace('static s16 sAdpcmTable[8][2][8];','static s16 sAdpcmTable[8][2][8];\nstatic int native_pcm_book=-1;')
        needle='    memcpy(sAdpcmTable, src, n);'
        assert text.count(needle)==1
        text=text.replace(needle,needle+'\n    native_pcm_book=native_audio_pcm_book(src,n);')
        needle='    while (nbytes > 0) {'
        assert text.count(needle)==1
        text=text.replace(needle,needle+'\n        if(native_audio_pcm_frame(native_pcm_book,in,out[-2],out[-1],out)) {\n            in+=9;out+=16;nbytes-=32;continue;\n        }')
        import dsp_audio
        text=dsp_audio.mixer(text)
        dest=BUILD/'overlay/mixer.c';dest.write_text(text);return dest
    if str(rel)=='src/audi.c':
        text='#include "native_audio.h"\n'+source.read_text()
        old='    osPiStartDma(&g_DmaIOMessageBuffer[g_NextDMa++], OS_MESG_PRI_HIGH, OS_READ, (u32)addr, freeBuffer, AUDIO_DMA_MAX_BUFFER_LENGTH, &g_DmaMessageQueue);'
        if text.count(old)!=1:raise ValueError('Original audio DMA boundary changed')
        text=text.replace(old,old+'\n    native_audio_swap_samples((u32)addr,freeBuffer,AUDIO_DMA_MAX_BUFFER_LENGTH);')
        text='#include "native_dsp_mix.h"\n#include "native_audio_pcm.h"\n#include "system.h"\n'+text
        render='    cmdlp = (Acmd*)alAudioFrame(g_AudioManager.cmdList[g_CurrentAcmdList], &g_CommandLength, outBuffer, info->frameSamples);'
        assert text.count(render)==1
        text=text.replace(render,'    native_dsp_mix_begin(info->data,info->frameSamples);\n    const uint64_t native_audio_start=sysGetMicroseconds();\n'+render+'\n    native_dsp_mix_end();'+'\n    static uint64_t native_audio_work,native_audio_peak;\n    static unsigned native_audio_frames;\n    const uint64_t native_audio_elapsed=sysGetMicroseconds()-native_audio_start;\n    native_audio_work+=native_audio_elapsed;if(native_audio_elapsed>native_audio_peak)native_audio_peak=native_audio_elapsed;\n    if(++native_audio_frames==300) {\n        uint32_t hits,misses;native_audio_pcm_stats(&hits,&misses);\n        sysLogPrintf(LOG_NOTE,"Audio work: avgUs=%u peakUs=%u PCMframes=%u decodeFallbacks=%u",(unsigned)(native_audio_work/300),(unsigned)native_audio_peak,hits,misses);\n        uint32_t dsp_spans,dsp_samples,dsp_fallbacks;native_dsp_mix_stats(&dsp_spans,&dsp_samples,&dsp_fallbacks);\n        sysLogPrintf(LOG_NOTE,"DSP dry mix: spans=%u samples=%u CPUfallbacks=%u",dsp_spans,dsp_samples,dsp_fallbacks);\n        native_audio_frames=0;native_audio_work=native_audio_peak=0;\n    }\n')
        dest=BUILD/'overlay/audi.c';dest.write_text(text)
        return dest
    if str(rel)=='src/game/title.c':
        text='#include "native_rle.h"\n#include "system.h"\n'+source.read_text()
        old='    rle_expand_8bit(dword_CODE_bss_80069588, dword_CODE_bss_8006958C);'
        new='    const u32 encodedSize=ALIGN64_V2(((u32)&unknown2_end - (u32)&unknown2));\n    if (*size < (s32)encodedSize || !native_rle_prepare(dword_CODE_bss_80069588,encodedSize,0x40400))\n        sysFatalError("Invalid or oversized original intro RLE image");\n    rle_expand_8bit(dword_CODE_bss_80069588, dword_CODE_bss_8006958C);'
        if text.count(old)!=1:raise ValueError('Original intro decoder boundary changed')
        text=text.replace(old,new)
        text='#include <ultra64.h>\nextern f32 portNativeAspect(void);\n'+text
        text=text.replace('(320.0f / 240.0f)','(portNativeAspect()>0?portNativeAspect():(320.0f/240.0f))')
        import intro_timing
        text=intro_timing.apply(text)
        dest=BUILD/'overlay/title.c';dest.write_text(text)
        return dest
    if str(rel)=='src/music.c':
        text=source.read_text()
        for track in (1,2,3):
            start=text.index(f'void musicTrack{track}FadeIn(')
            end=text.index('\n}',start)+2
            body=text[start:end]
            old=f'        alCSPPlay(g_musicXTrack{track}SeqPlayer);'
            if body.count(old)!=1:raise ValueError('Original fade-in boundary changed')
            body=body.replace(old,f'        if (!g_sndBootswitchSound) alCSPPlay(g_musicXTrack{track}SeqPlayer);')
            text=text[:start]+body+text[end:]
        start=text.index('void musicFadeTick(void)')
        end=text.index('\n}',start)+2
        body=text[start:end]
        if body.count('if (t0 == 0)')!=3:raise ValueError('Original fade-stop boundary changed')
        body=body.replace('if (t0 == 0)','if (t0 == 0 && !g_sndBootswitchSound)')
        text=text[:start]+body+text[end:]
        for track in (1,2,3):
            old=f'    alCSPSetVol(g_musicXTrack{track}SeqPlayer, t1);'
            if text.count(old)!=1:raise ValueError('Original track volume boundary changed')
            text=text.replace(old,'    /* Muted boot never creates sequence players. Retain the volume\n     * setting above, but do not enqueue an event on a missing player. */\n    if (g_sndBootswitchSound) return;\n'+old)
        for track in (1,2,3):
            old=f'    while (alCSPGetState(g_musicXTrack{track}SeqPlayer))\n        ;'
            if text.count(old)!=1:raise ValueError('Original music stop wait changed')
            text=text.replace(old,f'    native_music_wait_stopped(g_musicXTrack{track}SeqPlayer,{track});')
        text='#include "system.h"\n#include <PR/libaudio.h>\nextern int videoQuitRequested(void);\nstatic void native_music_wait_stopped(ALCSPlayer *player,int track) {\n    const uint64_t start=sysGetMicroseconds();\n    unsigned waited=0;\n    while(alCSPGetState(player)) {\n        if(!waited)sysLogPrintf(LOG_NOTE,"Music track %d: waiting for audio stop",track);\n        if(videoQuitRequested())for(;;)sysSleep(100000);\n        if(sysGetMicroseconds()-start>2000000)\n            sysFatalError("Music track %d stop was not acknowledged by audio thread",track);\n        sysSleep(1000); /* Let the equal-priority audio worker process STOP. */\n        ++waited;\n    }\n    if(waited)sysLogPrintf(LOG_NOTE,"Music track %d: audio stop acknowledged after %u yields",track,waited);\n}\n'+text
        text='#include "native_audio.h"\n#include "native_inflate.h"\n'+text
        old='    struct huft hlist;'
        if text.count(old)!=3:raise ValueError('Original music decompression workspaces changed')
        text=text.replace(old,'    struct huft hlist[NATIVE_HUFT_ENTRIES];')
        old='    decompressdata(temp_a0, thing.seqData, &hlist);'
        if text.count(old)!=3:raise ValueError('Original music decompression calls changed')
        text=text.replace(old,'    decompressdata(temp_a0, thing.seqData, hlist);')
        for name in ('sfx','instrument'):
            needle=f'        romCopy({name}Bank, &_{"sfx" if name=="sfx" else "instruments"}ctlSegmentRomStart, size);'
            if text.count(needle)!=1:raise ValueError('Original audio bank boundary changed')
            text=text.replace(needle,needle+f'\n        native_audio_bank_fixup({name}Bank,size,(u32)&_{"sfx" if name=="sfx" else "instruments"}tblSegmentRomStart);')
        dest=BUILD/'overlay/music.c';dest.parent.mkdir(exist_ok=True);dest.write_text(text)
        (dest.parent/'music.patch').write_text(''.join(difflib.unified_diff(source.read_text().splitlines(True),text.splitlines(True),fromfile='upstream/src/music.c',tofile='native/src/music.c')))
        return dest
    if str(rel)=='src/game/lv.c':
        text=source.read_text()
        start=text.index('void lvlStageLoad(s32 stage)')
        end=text.index('\ns32 lvlGetCurrentStageToLoad',start)
        body=text[start:end]
        body=re.sub(r'^(    +)([a-zA-Z_]\w*)\(([^;\n]*)\);$',lambda m:m[1]+'osSyncPrintf("Original stage: '+m[2]+'\\n");\n'+m[0],body,flags=re.M)
        text=text[:start]+body+text[end:]
        old='            DL = skyRender(DL);'
        if text.count(old)!=1:raise ValueError('Original sky render boundary changed')
        text=text.replace(old,'            gDPSetScissor(DL++, G_SC_NON_INTERLACE, viGetViewLeft(), viGetViewTop(), viGetViewLeft()+viGetViewWidth(), viGetViewTop()+viGetViewHeight()); /* Keep sky inside the player viewport, after border clear. */\n'+old)
        at=text.index('void lvlStageLoad(s32 stage)')
        text=text[:at]+'#include "native_distance_fog_emit.h"\n'+text[at:]
        old='            DL = skyRender(DL);'
        assert text.count(old)==1
        text=text.replace(old,'            struct NativeDistanceFog native_fog;\n            DL=native_distance_fog_current(&native_fog)?native_distance_fog_backdrop(DL):skyRender(DL);')
        import dynamic_stereo
        text=dynamic_stereo.level(text)
        dest=BUILD/'overlay/lv.c';dest.parent.mkdir(exist_ok=True);dest.write_text(text)
        return dest
    if str(rel)=='src/game/indy_comms.c':
        text=source.read_text()
        start=text.index('s32 indycommInit(void) {')
        end=text.index('\nvoid indycomm_removed',start)
        text=text[:start]+'''s32 indycommInit(void) {
    /* No Indy development workstation transport exists on this platform.
     * Leave the original host-operation guards disconnected; do not send an
     * initialization packet or pretend a host transfer succeeded. */
    indy_ready = 0;
    return 0;
}
'''+text[end:]
        dest=BUILD/'overlay/indy_comms.c';dest.parent.mkdir(exist_ok=True);dest.write_text(text)
        (dest.parent/'indy_comms.patch').write_text(''.join(difflib.unified_diff(source.read_text().splitlines(True),text.splitlines(True),fromfile='upstream/src/game/indy_comms.c',tofile='native/src/game/indy_comms.c')))
        return dest
    if str(rel)=='port/fast3d/gfx_pc.cpp':
        text='#include "native_cull.h"\n'+source.read_text()
        changes={
            'Gfx* overlay = optionsOverlayEmit();':'Gfx* overlay = nullptr; /* No desktop options overlay on 3DS. */',
            'static inline void *seg_addr(uintptr_t w1) {':'static inline void *seg_addr(uintptr_t w1) {\n    w1 = native_ram_translate_kseg(w1);',
            'segmentPointers[(offset >> 2) & 0xff] = (data < PORT_DRAM_SIZE) ? (data + PORT_DRAM_K0_BASE) : data;':'segmentPointers[(offset >> 2) & 0xff] = native_ram_translate_kseg(data);',
            'if (w1 < PORT_DRAM_SIZE) {\n        return (void *)(w1 + PORT_DRAM_K0_BASE);\n    }':'/* Untagged addresses are live ARM pointers, including low image/BSS. */',
        }
        for old,new in changes.items():
            if text.count(old)!=1:raise ValueError('Upstream graphics adapter changed: '+old)
            text=text.replace(old,new)
        old='if (!g_safe_area_crop_enabled && !g_split_screen && !g_overlay_window_space) {'
        if text.count(old)!=1:raise ValueError('Desktop scissor edge trim changed')
        text=text.replace(old,'if (false) { /* Native LCD has no CRT/desktop overscan trim. */')
        start=text.index('        float dx1 = v1->x / (v1->w)')
        end=text.index('        // If inverted culling is requested',start)
        text=text[:start]+"        float cross=native_triangle_facing(v1->x,v1->y,v1->w,\n                                         v2->x,v2->y,v2->w,\n                                         v3->x,v3->y,v3->w);\n\n"+text[end:]
        text='#include "native_settings.h"\n#include "native_theme.h"\n#include "native_unorm.h"\nextern "C" void native_pica_set_watch(int) __attribute__((weak));\nextern "C" void native_pica_set_watch_tabs(int) __attribute__((weak));\nextern "C" void native_pica_set_watch_body(int) __attribute__((weak));\nextern "C" void native_pica_watch_canvas(float,float) __attribute__((weak));\nextern "C" void native_pica_watch_rect(float,float,float,float,int) __attribute__((weak));\nextern "C" int native_pica_ui_rect(int) __attribute__((weak));\nstatic int native_watch_theme;\n'+text
        # Preserve the original UI rectangles BEFORE top-screen aspect/camera
        # adjustments. The lower LCD lays these out in its own pixel canvas.
        a=text.index('static void gfx_draw_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {')
        b=text.index('/* D226:',a)
        rectangle=text[a:b]
        needle='    float lryf = lry;'
        assert rectangle.count(needle)==1
        rectangle=rectangle.replace(needle,needle+'\n    const bool native_stereo_rect=native_pica_ui_rect && native_pica_ui_rect(-1);\n    if(native_stereo_rect){gfx_flush();native_pica_ui_rect(1);}\n    const bool native_watch_rect=(rsp.extra_geometry_mode & NATIVE_WATCH_SCOPE) && !(rsp.extra_geometry_mode & NATIVE_WATCH_BODY_SCOPE) && native_pica_watch_rect;\n    if(native_watch_rect){gfx_flush();native_pica_watch_rect(ulxf*.25f,ulyf*.25f,lrxf*.25f,lryf*.25f,1);}')
        needle='    rsp.geometry_mode = geometry_mode_saved;'
        assert rectangle.count(needle)==1
        rectangle=rectangle.replace(needle,'    if(native_watch_rect){gfx_flush();native_pica_watch_rect(0,0,0,0,0); }\n    if(native_stereo_rect){gfx_flush();native_pica_ui_rect(0);}\n'+needle)
        text=text[:a]+rectangle+text[b:]
        a=text.index('static inline void gfx_update_aspect_mode(void) {')
        b=text.index('\nstatic void gfx_sp_extra_geometry_mode',a)
        aspect=text[a:b];end=aspect.rfind('}')
        aspect=aspect[:end]+'    if((rsp.extra_geometry_mode & NATIVE_WATCH_SCOPE) && native_pica_watch_canvas)native_pica_watch_canvas(gfx_current_dimensions.aspect_ratio/rsp.aspect_scale,-rsp.aspect_ofs);\n'+aspect[end:]
        text=text[:a]+aspect+text[b:]
        old='static void gfx_sp_extra_geometry_mode(uint32_t clear, uint32_t set) {'
        text=text.replace(old,old+'\n    unsigned next=(rsp.extra_geometry_mode & ~clear)|set;\n    if((next^rsp.extra_geometry_mode)&NATIVE_WATCH_SCOPE) {\n        gfx_flush();native_watch_theme=native_setting_watch();\n        if(native_pica_set_watch)native_pica_set_watch((next&NATIVE_WATCH_SCOPE)!=0);\n    }\n    if((next^rsp.extra_geometry_mode)&NATIVE_WATCH_TABS_SCOPE) {\n        gfx_flush();if(native_pica_set_watch_tabs)native_pica_set_watch_tabs((next&NATIVE_WATCH_TABS_SCOPE)!=0);\n    }\n    if((next^rsp.extra_geometry_mode)&NATIVE_WATCH_BODY_SCOPE) {\n        gfx_flush();if(native_pica_set_watch_body)native_pica_set_watch_body((next&NATIVE_WATCH_BODY_SCOPE)!=0);\n    }')
        old='                    buf_vbo[buf_vbo_len++] = color->r / 255.0f;\n                    buf_vbo[buf_vbo_len++] = color->g / 255.0f;\n                    buf_vbo[buf_vbo_len++] = color->b / 255.0f;'
        new='                    unsigned r=color->r,g=color->g,b=color->b;\n                    if(rsp.extra_geometry_mode & NATIVE_WATCH_SCOPE)native_theme_rgb(native_watch_theme,&r,&g,&b);\n                    buf_vbo[buf_vbo_len++] = r / 255.0f;\n                    buf_vbo[buf_vbo_len++] = g / 255.0f;\n                    buf_vbo[buf_vbo_len++] = b / 255.0f;'
        if text.count(old)!=1:raise ValueError('Original RGB material packing changed')
        text=text.replace(old,new)
        old='            buf_vbo[buf_vbo_len++] = rdp.fog_color.r / 255.0f;\n            buf_vbo[buf_vbo_len++] = rdp.fog_color.g / 255.0f;\n            buf_vbo[buf_vbo_len++] = rdp.fog_color.b / 255.0f;'
        new='            unsigned r=rdp.fog_color.r,g=rdp.fog_color.g,b=rdp.fog_color.b;\n            if(rsp.extra_geometry_mode & NATIVE_WATCH_SCOPE)native_theme_rgb(native_watch_theme,&r,&g,&b);\n            buf_vbo[buf_vbo_len++] = r / 255.0f;\n            buf_vbo[buf_vbo_len++] = g / 255.0f;\n            buf_vbo[buf_vbo_len++] = b / 255.0f;'
        if text.count(old)!=1:raise ValueError('Original fog material packing changed')
        text=text.replace(old,new)
        text='#include "native_rsp_profile.h"\n'+text
        for signature,stage in (
          ('static void gfx_sp_matrix(uint8_t parameters, const int32_t* addr) {','matrix'),
          ('static void gfx_sp_vertex(size_t n_vertices, size_t dest_index, const Vtx* vertices) {','vertex'),
          ('static void gfx_sp_tri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {','triangle')):
            if text.count(signature)!=1:raise ValueError('Original RSP profiling boundary changed')
            extra='\n    native_rsp_profile::Scope profile(native_rsp_profile::'+stage+');'
            if stage=='vertex':extra+='\n    if(native_rsp_profile::enabled)native_rsp_profile::vertices+=n_vertices;'
            text=text.replace(signature,signature+extra)
        old='    gfx_rapi->start_frame();'
        if text.count(old)!=1:raise ValueError('Original frame boundary changed')
        text=text.replace(old,old+'\n    static unsigned native_profile_frame;\n    static const bool native_profile_all=getenv("EOG_GRAPHICS_TRACE")!=nullptr;\n    native_rsp_profile::reset(native_profile_frame++%120==0 || native_profile_all);')
        old='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
        if text.count(old)!=1:raise ValueError('Original RSP frame finish changed')
        text=text.replace(old,'    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"RSP stages (sampled wall time): matrices=%.2fms/%llu vertices=%.2fms/%llu loaded=%llu triangles=%.2fms/%llu batches=%.2fms/%llu (batch includes fallback/backend)",native_rsp_profile::milliseconds(native_rsp_profile::matrix),(unsigned long long)native_rsp_profile::calls[0],native_rsp_profile::milliseconds(native_rsp_profile::vertex),(unsigned long long)native_rsp_profile::calls[1],(unsigned long long)native_rsp_profile::vertices,native_rsp_profile::milliseconds(native_rsp_profile::triangle),(unsigned long long)native_rsp_profile::calls[2],native_rsp_profile::milliseconds(native_rsp_profile::batch),(unsigned long long)native_rsp_profile::calls[3]);\n'+old)
        text='#include "native_aa.h"\n'+text
        start=text.index('static void gfx_sp_tri1(uint8_t')
        end=text.index('static inline void gfx_sp_tri4',start)
        body=text[start:end].replace('rdp.other_mode_l','native_render_mode')
        opening=body.index('{')+1
        body=body[:opening]+'\n    const uint32_t native_render_mode=native_aa_render_mode(rdp.other_mode_l,native_setting_aa());'+body[opening:]
        text=text[:start]+body+text[end:]
        text=re.sub(r'(rdp\.grayscale_color\.[rgba]|color->a|(?<![\w.])r|(?<![\w.])g|(?<![\w.])b) / 255\.0f',r'native_unorm8(\1)',text)
        from triangle_setup import apply as cache_triangle_setup
        text=cache_triangle_setup(text)
        from vertex_pack import apply as cache_vertex_pack
        text=cache_vertex_pack(text)
        import dynamic_stereo
        text=dynamic_stereo.interpreter(text)
        from viewmodel_cull import apply as viewmodel_cull
        text=viewmodel_cull(text)
        import glass_detail
        text=glass_detail.apply(text)
        import uv_prepare
        text=uv_prepare.apply(text)
        import distance_fog
        text=distance_fog.apply(text)
        import fog_batch
        text=fog_batch.apply(text)
        import scenery_cache
        text=scenery_cache.apply(text)
        import scenery_batches
        text=scenery_batches.apply(text)
        import mesh_lod
        text=mesh_lod.apply(text)
        import gpu_world
        text=gpu_world.apply(text)
        import lod_hint
        text=lod_hint.apply(text)
        import cpu_clip
        text=cpu_clip.apply(text)
        import vertex_reuse
        text=vertex_reuse.apply(text)
        import vertex_refs
        text=vertex_refs.apply(text)
        import static_attributes
        text=static_attributes.apply(text)
        import prepared_batches
        text=prepared_batches.apply(text)
        import model_batches
        text=model_batches.apply(text)
        import retained_batches
        text=retained_batches.apply(text)
        import simple_glass_batches
        text=simple_glass_batches.apply(text)
        import deferred_background
        text=deferred_background.apply(text)
        import model_face_cull
        text=model_face_cull.apply(text)
        import cpu_world_indices
        text=cpu_world_indices.apply(text)
        import deferred_cpu_rooms
        text=deferred_cpu_rooms.apply(text)
        import room_packets
        text=room_packets.apply(text)
        import material_work
        text=material_work.apply(text)
        import quad_batches
        text=quad_batches.apply(text)
        dest=BUILD/'overlay/gfx_pc.cpp';dest.parent.mkdir(exist_ok=True);dest.write_text(text)
        (dest.parent/'gfx_pc.patch').write_text(''.join(difflib.unified_diff(source.read_text().splitlines(True),text.splitlines(True),fromfile='upstream/port/fast3d/gfx_pc.cpp',tofile='native/port/fast3d/gfx_pc.cpp')))
        return dest
    if str(rel)=='src/memp.c':
        text=source.read_text()
        old='        while (1);'
        if text.count(old)!=3:raise ValueError('Original allocator failure paths changed')
        text='#include "system.h"\n'+text.replace(old,'        sysFatalError("Original memory allocation failed: %u bytes, pool %u",bytes,poolnum);')
        dest=BUILD/'overlay/memp.c';dest.parent.mkdir(exist_ok=True);dest.write_text(text)
        (dest.parent/'memp.patch').write_text(''.join(difflib.unified_diff(source.read_text().splitlines(True),text.splitlines(True),fromfile='upstream/src/memp.c',tofile='native/src/memp.c')))
        return dest
    if str(rel)=='src/boss.c':
        text=source.read_text()
        start=text.index('void bossInitMainthreadData(void)')
        end=text.index('void bossEnableShowMemUseFlag',start)
        body=text[start:end]
        for call in ('image_entries_load','obInit','textInit','alloc_load_expand_ani_table','initGameData'):
            old='    '+call+'();'
            if body.count(old)!=1:raise ValueError('Original boot checkpoint changed: '+call)
            body=body.replace(old,'    sysLogPrintf(LOG_NOTE, "Original boot: '+call+'");\n'+old)
        text='#include <ultra64.h>\n#include "native_baked_lod.h"\n#include "system.h"\n'+text[:start]+body+text[end:]
        old='        lvlStageLoad(g_StageNum);'
        if text.count(old)!=1:raise ValueError('Original stage entry changed')
        text=text.replace(old,'        if(g_StageNum!=LEVELID_DAM && g_StageNum!=LEVELID_FACILITY && g_StageNum!=LEVELID_RUNWAY && g_StageNum!=LEVELID_SURFACE && g_StageNum!=LEVELID_BUNKER1 && g_StageNum!=LEVELID_SILO && g_StageNum!=LEVELID_FRIGATE && g_StageNum!=LEVELID_SURFACE2 && g_StageNum!=LEVELID_BUNKER2 && g_StageNum!=LEVELID_STATUE && g_StageNum!=LEVELID_ARCHIVES && g_StageNum!=LEVELID_STREETS && g_StageNum!=LEVELID_DEPOT && g_StageNum!=LEVELID_TRAIN && g_StageNum!=LEVELID_JUNGLE && g_StageNum!=LEVELID_CONTROL && g_StageNum!=LEVELID_CAVERNS && g_StageNum!=LEVELID_CRADLE && g_StageNum!=LEVELID_AZTEC && g_StageNum!=LEVELID_EGYPT && g_StageNum!=LEVELID_TEMPLE && g_StageNum!=LEVELID_COMPLEX && g_StageNum!=LEVELID_CAVES && g_StageNum!=LEVELID_LIBRARY && g_StageNum!=LEVELID_BASEMENT && g_StageNum!=LEVELID_STACK && g_StageNum!=LEVELID_TITLE) sysFatalError("This mission is not enabled in this prototype");\n        if(native_baked_lod_reset)native_baked_lod_reset();\n        if(native_assets_stage_skin)native_assets_stage_skin(g_StageNum==LEVELID_DAM || g_StageNum==LEVELID_FACILITY || g_StageNum==LEVELID_RUNWAY || g_StageNum==LEVELID_SURFACE || g_StageNum==LEVELID_BUNKER1 || g_StageNum==LEVELID_SILO || g_StageNum==LEVELID_FRIGATE || g_StageNum==LEVELID_SURFACE2 || g_StageNum==LEVELID_BUNKER2 || g_StageNum==LEVELID_STATUE || g_StageNum==LEVELID_ARCHIVES || g_StageNum==LEVELID_STREETS || g_StageNum==LEVELID_DEPOT || g_StageNum==LEVELID_TRAIN || g_StageNum==LEVELID_JUNGLE || g_StageNum==LEVELID_CONTROL || g_StageNum==LEVELID_CAVERNS || g_StageNum==LEVELID_CRADLE || g_StageNum==LEVELID_AZTEC || g_StageNum==LEVELID_EGYPT,sysGetMicroseconds());\n        sysLogPrintf(LOG_NOTE,"Original stage load begin: %d",g_StageNum);\n'+old+'\n        sysLogPrintf(LOG_NOTE,"Original stage load complete: %d",g_StageNum);')
        text='#include "native_work_profile.h"\n'+text
        loop='        while (g_MainStageNum < 0 || pendingGfx != 0)'
        assert text.count(loop)==1
        text=text.replace(loop,'        uint64_t native_main_active_start=0;\n'+loop)
        receive='                osRecvMesg(&gfxFrameMsgQ, (OSMesg *)&localGfxFrameMsg, OS_MESG_BLOCK);'
        assert text.count(receive)==1
        text=text.replace(receive,'''                native_work_profile_end(NATIVE_WORK_MAIN,native_main_active_start);
                uint64_t native_main_wait_start=native_work_profile_begin();
'''+receive+'''
                native_work_profile_end(pendingGfx?NATIVE_WORK_WAIT_GFX:NATIVE_WORK_WAIT_VI,native_main_wait_start);
                native_main_active_start=native_work_profile_begin();''')
        load='        lvlStageLoad(g_StageNum);'
        assert text.count(load)==1
        text=text.replace(load,'        uint64_t native_load_start=native_work_profile_begin();\n'+load+'\n        native_work_profile_end(NATIVE_WORK_STAGE_LOAD,native_load_start);')
        text='extern void native_game_scene_built(unsigned long long) __attribute__((weak));\n'+text
        text='#include "native_guard_activity.h"\n#include "native_assets.h"\n'+text
        text=text.replace('        lvlStageLoad(g_StageNum);','        native_guard_activity_reset();\n        lvlStageLoad(g_StageNum);')
        old='                            gdl = lvlRender(gdl);'
        assert text.count(old)==1
        text=text.replace(old,'                            uint64_t native_scene_profile=native_work_profile_begin();\n                            unsigned long long native_scene_start=native_game_scene_built?sysGetMicroseconds():0;\n'+old+'\n                            native_work_profile_end(NATIVE_WORK_SCENE,native_scene_profile);\n                            if(native_game_scene_built)native_game_scene_built(sysGetMicroseconds()-native_scene_start);')
        dest=BUILD/'overlay/boss.c';dest.parent.mkdir(exist_ok=True);dest.write_text(text)
        (dest.parent/'boss.patch').write_text(''.join(difflib.unified_diff(source.read_text().splitlines(True),text.splitlines(True),fromfile='upstream/src/boss.c',tofile='native/src/boss.c')))
        return dest
    if str(rel)=='src/game/model.c':
        text=source.read_text()
        old='    if (model->unka0 && !sub_GAME_7F01FC10(model, &rw->Header.pos, &sp2c, &rw->Header.ground))'
        if text.count(old)!=1:raise ValueError('Original model collision callback changed')
        text=text.replace(old,'    if (model->unka0 && !sub_GAME_7F01FC10)\n        sysFatalError("Native model: character collision callback is not linked");\n'+old)
        text='#include "system.h"\n'+text
        marker='#include "chr.h"'
        text=text.replace(marker,marker+'\n/* Gait-only link omits character movement. Full game resolves this original callback. */\nextern s32 sub_GAME_7F01FC10(Model *, coord3d *, coord3d *, f32 *) __attribute__((weak));')
        text='struct ModelNode;\nextern int native_watch_skin_active __attribute__((weak));\nextern int native_watch_custom_part(const struct ModelNode *) __attribute__((weak));\n'+text
        for signature,arg in (('void modelRenderNodeGundl(ModelRenderData* renderdata, ModelNode* arg1)','arg1'),('void modelRenderNodeDl(ModelRenderData *renderdata, Model *model, ModelNode *node)','node')):
            old=signature+'\n{';assert text.count(old)==1
            text=text.replace(old,old+f'\n    if(&native_watch_skin_active && native_watch_skin_active && native_watch_custom_part && native_watch_custom_part({arg}))return;')
        text=text.replace('#include "chr.h"','#include "chr.h"\n#include "native_baked_lod.h"')
        old='                gSPDisplayList(renderdata->gdl++, rwdata->DisplayListCollisions.gdl);'
        assert text.count(old)==1
        text=text.replace(old,'                Gfx *native_before=renderdata->gdl;\n                if(native_baked_lod_begin && &native_object_lod_tier && native_object_lod_tier)renderdata->gdl=native_baked_lod_begin(renderdata->gdl,rwdata->DisplayListCollisions.Vertices,rodata->DisplayListCollisions.numVertices);\n                int native_lod_on=renderdata->gdl!=native_before;\n'+old+'\n                if(native_lod_on)native_baked_lod_command(renderdata->gdl++,(struct NativeBakedLodPacket *)NULL);')
        import guard_pose
        text=guard_pose.apply(text)
        dest=BUILD/'overlay/model.c';dest.parent.mkdir(exist_ok=True);dest.write_text(text)
        (dest.parent/'model.patch').write_text(''.join(difflib.unified_diff(source.read_text().splitlines(True),text.splitlines(True),fromfile='upstream/src/game/model.c',tofile='native/src/game/model.c')))
        return dest
    if str(rel)=='src/game/stan.c':
        text=source.read_text()
        changes={
            'savedPointIndex = uninitialized;':'savedPointIndex = 0; /* Defined failure diagnostic before first crossing. */',
            'while (((*((u32 *) tile)) != 0) && (tile->room == room))\n                {':'while (((*((u32 *) tile)) != 0) && (tile->room == room))\n                {\n                    nearEdge = 0; /* Reset per tile, including first candidate. */',
        }
        for old,new in changes.items():
            if text.count(old)!=1:raise ValueError('Original STAN boundary changed: '+old)
            text=text.replace(old,new)
        dest=BUILD/'overlay/stan.c';dest.parent.mkdir(exist_ok=True);dest.write_text(text)
        (dest.parent/'stan.patch').write_text(''.join(difflib.unified_diff(source.read_text().splitlines(True),text.splitlines(True),fromfile='upstream/src/game/stan.c',tofile='native/src/game/stan.c')))
        return dest
    if str(rel)=='src/game/image.c':
        text=source.read_text()
        old='u8 scratch[0x2100];'
        if text.count(old)!=1:raise ValueError('Texture inflate workspace changed upstream')
        text=text.replace(old,'u8 scratch[0x2100 * (__SIZEOF_POINTER__ / 4)] __attribute__((aligned(8))); /* huft is 8B on ARM32, 16B on host64. */')
        dest=BUILD/'overlay/image.c';dest.write_text(text)
        (dest.parent/'image.patch').write_text(''.join(difflib.unified_diff(source.read_text().splitlines(True),text.splitlines(True),fromfile='upstream/src/game/image.c',tofile='native/src/game/image.c')))
        return dest
    if str(rel)!='port/src/libultra.c':
        # Quoted headers resolve beside the source before -I directories. Copy
        # every consumer beside the generated portable model-slot header.
        if re.search(r'#include ["<]objecthandler.h[">]',source.read_text()):
            dest=BUILD/'overlay'/source.name;dest.write_text(source.read_text());return dest
        return source
    # Platform-only adaptations; original game sources and checkout stay intact.
    text='#include <stdio.h> /* Explicit host API; SDL no longer imports it. */\n#include "native_cart.h"\n'+source.read_text()
    changes={
        '#define PORT_THREAD_STACK (8u * 1024u * 1024u)':'#define PORT_THREAD_STACK (128u * 1024u)',
        'u32 osMemSize   = 16 * 1024 * 1024;':'u32 osMemSize   = 8 * 1024 * 1024;',
        'if (mq == &gfxFrameMsgQ) watchSettingsGameTick();':'/* Native baseline excludes the desktop F10 settings bridge. */',
        'memcpy(dstVA, (const void *)(uintptr_t)srcPA, size);':'if (!native_cart_read(srcPA, dstVA, size))\n            sysFatalError("Native cartridge read failed: %08x + %u", srcPA, size);',
        'static const uintptr_t bases[2] = { PORT_DRAM_V1_BASE, PORT_DRAM_K0_BASE };':'const uintptr_t bases[2] = { PORT_DRAM_V1_BASE, PORT_DRAM_K0_BASE };',
    }
    for old,new in changes.items():
        if text.count(old)!=1:raise ValueError('Upstream OS adapter changed: '+old)
        text=text.replace(old,new)
    text='#if defined(__3DS__)\nextern void native_platform_crash_init(void) __attribute__((weak));\nstatic void native_worker_crash_init(void) { if(native_platform_crash_init) native_platform_crash_init(); }\n#else\nstatic void native_worker_crash_init(void) {}\n#endif\n'+text
    for name in ('portThreadWrapper','portRenderWorker','portTickThread'):
        needle=f'static void *{name}(void *arg)\n{{'
        if text.count(needle)!=1:raise ValueError('Original worker entry changed: '+name)
        text=text.replace(needle,needle+'\n    native_worker_crash_init();\n#if defined(__3DS__)\n    extern void native_platform_worker_info(const char *) __attribute__((weak));\n    if(native_platform_worker_info)native_platform_worker_info("'+name+'");\n#endif')
    text='#include "native_work_profile.h"\n'+text
    from os_queue_patch import patch_os_queues
    text=patch_os_queues(text)
    at=text.index('static void *portRenderWorker(void *arg)')
    end=text.index('static int portRenderWorkerOn(void)',at)
    worker=text[at:end]
    old='        pthread_mutex_lock(&s_rwLock);'
    assert worker.count(old)==2
    worker=worker.replace(old,'        uint64_t native_idle_start=native_work_profile_begin();\n'+old,1)
    old='        portRenderGfxTask(dl);'
    assert worker.count(old)==1
    worker=worker.replace(old,'''        native_work_profile_end(NATIVE_WORK_RENDER_IDLE,native_idle_start);
        uint64_t native_render_start=native_work_profile_begin();
'''+old+'''
        native_work_profile_end(NATIVE_WORK_RENDER,native_render_start);''')
    text=text[:at]+worker+text[end:]
    old='''            if (pthread_create(&th, &attr, portRenderWorker, NULL) == 0) {
                pthread_detach(th);'''
    new='''            int native_started=0;
#if defined(__3DS__)
            extern int native_platform_start_render(void *(*)(void *),void *,size_t) __attribute__((weak));
            if(native_platform_start_render)native_started=native_platform_start_render(portRenderWorker,NULL,PORT_THREAD_STACK);
#endif
            if (native_started || pthread_create(&th, &attr, portRenderWorker, NULL) == 0) {
                if(!native_started)pthread_detach(th);'''
    if text.count(old)!=1:raise ValueError('Original render worker creation changed')
    text=text.replace(old,new)
    old='    rc = pthread_create(&pt->th, &attr, portThreadWrapper, pt);'
    new='''    int native_audio_started=0;
#if defined(__3DS__)
    extern int native_platform_start_audio(void *(*)(void *),void *,size_t) __attribute__((weak));
    if(pt->id==4 /* AUDI_THREAD_ID */ && native_platform_start_audio)
        native_audio_started=native_platform_start_audio(portThreadWrapper,pt,PORT_THREAD_STACK);
#endif
    rc=native_audio_started?0:pthread_create(&pt->th, &attr, portThreadWrapper, pt);'''
    if text.count(old)!=1:raise ValueError('Original audio thread creation changed')
    text=text.replace(old,new)
    dest=BUILD/'overlay/libultra.c';dest.parent.mkdir(exist_ok=True);dest.write_text(text)
    (dest.parent/'libultra.patch').write_text(''.join(difflib.unified_diff(source.read_text().splitlines(True),text.splitlines(True),fromfile='upstream/port/src/libultra.c',tofile='native/port/src/libultra.c')))
    return dest
def runtime_symbols():
    # Original animation records link at VMA zero, so these are offsets, not
    # host pointers. Keep exact values from the authoritative upstream header.
    header=(UP/'assets/animationtable_data.h').read_text()
    values=re.findall(r'^#define PTR_ANIM_(\w+)\s+(0x[0-9a-fA-F]+|[0-9]+)\s*$',header,re.M)
    lines=['.section .data','.global _bssSegmentEnd','.set _bssSegmentEnd, 0x80050000',
           '.global ANIM_DATA_empty','.set ANIM_DATA_empty, 0']
    for name,value in values:lines += ['.global ANIM_DATA_'+name,'.set ANIM_DATA_'+name+', '+value]
    path=BUILD/'runtime_symbols.s';path.write_text('\n'.join(lines)+'\n');return path
def source_files():
    return [Path(s) for s in json.loads((TARGET/'source-files.json').read_text())]
def compile_one(rel):
    src=source_path(rel);obj=BUILD/'obj'/rel.with_suffix('.o');obj.parent.mkdir(parents=True,exist_ok=True)
    cpp=rel.suffix=='.cpp';compiler=CC.with_name('arm-none-eabi-g++') if cpp else CC
    flags=[s for s in FLAGS if not cpp or s not in ('-std=gnu11','-fplan9-extensions','-fpermissive')]
    if cpp:flags+=['-std=gnu++20','-fno-rtti','-fno-exceptions']
    if str(rel).startswith('port/fast3d/'):flags+=['-O2']
    defs=[s for s in DEFS if not cpp or not any(s.startswith(n+'=') for n in GAME_LIBC)]
    system=['-I'+str(BUILD/'include/libultra-system')] if str(rel)=='port/src/libultra.c' else []
    if cpp:system+=['-I'+str(UP/'port/fast3d')]
    args=[str(compiler),*flags,*['-D'+s for s in defs],*system,*['-I'+str(p) for p in INCLUDES],'-include',str(TARGET/'include/prelude.h'),'-include','versioninfo.h']
    if not cpp:
        import rom_data_plan
        src=rom_data_plan.rewrite(rel,src,args)
    args+=['-save-temps=obj','-c',str(src),'-o',str(obj)]
    result=subprocess.run(args,capture_output=True,text=True)
    if result.returncode==0 and not cpp:
        import original_string_plan
        original_string_plan.patch_object(rel,obj)
    log=BUILD/'logs'/str(rel).replace('/','__');log=log.with_suffix('.log');log.write_text(result.stdout+result.stderr)
    return {'file':str(rel),'compiler_flags':flags,'ok':result.returncode==0,'object':str(obj.relative_to(TARGET)),'log':str(log.relative_to(TARGET)),'source_sha256':hashlib.sha256((UP/rel).read_bytes()).hexdigest(),'compiled_source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'errors':(result.stdout+result.stderr)[:1600] if result.returncode else ''}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--jobs',type=int,default=4);p.add_argument('--file',action='append');a=p.parse_args();prepare();files=[Path(f) for f in a.file] if a.file else source_files();start=time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:results=list(pool.map(compile_one,files))
    report={'target':'ARMv6K Nintendo 3DS','upstream':'fda76b69837e6c5aae436e0d3e798989e49b06d2','compiler':subprocess.check_output([str(CC),'--version'],text=True).splitlines()[0],'flags':FLAGS,'defines':DEFS,'total':len(results),'compiled':sum(x['ok'] for x in results),'seconds':round(time.monotonic()-start,2),'files':results}
    (BUILD/('partial-compile-report.json' if a.file else 'compile-report.json')).write_text(json.dumps(report,indent=2)+'\n')
    print(f"Original engine: {report['compiled']}/{report['total']} compiled for ARM in{report['seconds']}s")
    for r in results:
        if not r['ok']:print(r['file']+'\n'+r['errors'][:550])
    if not a.file and report['compiled']==report['total']:
        subprocess.run([str(CC.with_name('arm-none-eabi-ar')),'rcs',str(BUILD/'liboriginal-engine.a'),*[str(TARGET/r['object']) for r in results]],check=True)
        gfx=[compile_one(Path('port/fast3d')/f'{name}.cpp') for name in ('gfx_pc','gfx_cc')]
        (BUILD/'graphics-compile-report.json').write_text(json.dumps(gfx,indent=2)+'\n')
        for r in gfx:
            if not r['ok']:print(r['file']+'\n'+r['errors'])
        if not all(r['ok'] for r in gfx):return 1
        subprocess.run([str(CC.with_name('arm-none-eabi-ar')),'rcs',str(BUILD/'liboriginal-rsp.a'),*[str(TARGET/r['object']) for r in gfx]],check=True)
        print('Original Fast3D interpreter and combiner compiled for ARM (rendering backend not yet connected).')
        platform=compile_one(Path('port/src/libultra.c'))
        (BUILD/'platform-compile-report.json').write_text(json.dumps(platform,indent=2)+'\n')
        if not platform['ok']:print(platform['errors']);return 1
        subprocess.run([str(CC.with_name('arm-none-eabi-ar')),'rcs',str(BUILD/'libnative-os.a'),str(TARGET/platform['object'])],check=True)
        print('OS adapter compiled against native pthread/libctru services (full boot not yet connected).')
        mixer=compile_one(Path('port/src/mixer.c'))
        (BUILD/'audio-compile-report.json').write_text(json.dumps(mixer,indent=2)+'\n')
        if not mixer['ok']:print(mixer['errors']);return 1
        subprocess.run([str(CC.with_name('arm-none-eabi-ar')),'rcs',str(BUILD/'libnative-mixer.a'),str(TARGET/mixer['object'])],check=True)
        print('CPU audio mixer compiled for ARM (native audio output not yet connected).')
        drawdist=compile_one(Path('port/src/drawdistgameplay.c'))
        (BUILD/'drawdist-compile-report.json').write_text(json.dumps(drawdist,indent=2)+'\n')
        if not drawdist['ok']:print(drawdist['errors']);return 1
        baseline=BUILD/'baseline_options.o'
        subprocess.run([str(CC),*FLAGS,*['-D'+s for s in DEFS],*['-I'+str(p) for p in INCLUDES],'-include',str(TARGET/'include/prelude.h'),'-include','versioninfo.h','-c',str(TARGET/'src/baseline_options.c'),'-o',str(baseline)],check=True)
        extras=[]
        motion=BUILD/'native_motion_sensor.o'
        subprocess.run([str(CC.with_name('arm-none-eabi-g++')),'-std=gnu++20','-O2','-Wall','-Wextra','-Werror','-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-fno-rtti','-fno-exceptions','-ffunction-sections','-fdata-sections','-I'+str(TARGET/'include'),'-c',str(TARGET/'src/native_motion_sensor.cpp'),'-o',str(motion)],check=True)
        extras.append(str(motion))
        for name in ('native_settings.c','native_controls.c'):
            obj=BUILD/(name+'.o')
            subprocess.run([str(CC),*FLAGS,*['-D'+v for v in DEFS],*['-I'+str(v) for v in INCLUDES],'-include',str(TARGET/'include/prelude.h'),'-c',str(TARGET/'src'/name),'-o',str(obj)],check=True)
            extras.append(str(obj))
        subprocess.run([str(CC.with_name('arm-none-eabi-ar')),'rcs',str(BUILD/'libnative-baseline.a'),str(baseline),str(TARGET/drawdist['object']),*extras],check=True)
        fixups=BUILD/'rom_fixups.o'
        subprocess.run([str(CC),*FLAGS,*['-D'+s for s in DEFS],*['-I'+str(p) for p in INCLUDES],'-include',str(TARGET/'include/prelude.h'),'-include','versioninfo.h','-c',str(TARGET/'src/rom_fixups.c'),'-o',str(fixups)],check=True)
        subprocess.run([str(CC),'-std=gnu11','-O2','-Wall','-Wextra','-Werror','-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-D__3DS__','-I'+str(TARGET/'include'),'-I/opt/devkitpro/libctru/include','-c',str(TARGET/'src/input_3ds.c'),'-o',str(BUILD/'input_3ds.o')],check=True)
        print('Original-option defaults and native N64 controller bridge compiled.')
        symbols=BUILD/'romassets.o'
        subprocess.run([str(CC),'-march=armv6k','-c',str(UP/'port/src/romassets_u.s'),'-o',str(symbols)],check=True)
        subprocess.run([str(CC),'-march=armv6k','-c',str(runtime_symbols()),'-o',str(BUILD/'runtime_symbols.o')],check=True)
        print('Original US cartridge asset symbols assembled for ARM.')
    return int(report['compiled']!=report['total'])
if __name__=='__main__':raise SystemExit(main())
