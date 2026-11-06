#define _POSIX_C_SOURCE 200809L
#include "native_settings.h"
#include "native_menu_options.h"
#include "native_frame_limit.h"
#include "native_pad.h"
#include "native_theme.h"
#include "native_aa.h"
#include "native_console.h"
#include "native_aspect.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef NATIVE_TEST_FAT_RENAME
#include <errno.h>
#include <fcntl.h>
static unsigned replace_rejected;
int native_test_rename(const char *from,const char *to) {
    if(access(to,F_OK)==0){replace_rejected++;errno=EEXIST;return -1;}
    return renameat(AT_FDCWD,from,AT_FDCWD,to);
}
#endif
#include "native_adaptive_checks.h"
static void check_first_launch_defaults(void) {
    char directory[]="/tmp/eog-console-defaults-XXXXXX";assert(mkdtemp(directory));
    char path[512];snprintf(path,sizeof path,"%s/native-options.ini",directory);
    for(int model=-1;model<7;model++) {
        int newer=model==2 || model==4 || model==5;
        native_settings_init(directory);
        assert(native_settings_apply_console_defaults(model)==1);
        assert(native_setting_style()==(newer?8:9));
        assert(native_setting_aspect()==(newer?1:0));
        assert(native_setting_fps()==30);
        assert(native_setting_lod()==(newer?1:4) && native_setting_view_distance()==(newer?1:3));
        assert(native_aspect_width(native_setting_aspect())==(newer?400u:320u));
        assert(native_setting_game(0)==1 && native_setting_aa()==0);
        assert(native_setting_sharpness()==4 && native_setting_vignette()==1 && native_setting_vignette_mode()==1);
        assert(native_setting_glass()==(newer?0:1));
        assert(access(path,F_OK)==0); /* Save immediately, without opening watch. */
        native_settings_init(directory);
        assert(!native_settings_apply_console_defaults(newer?0:5));
        assert(native_setting_style()==(newer?8:9) && native_setting_aspect()==(newer?1:0));
        struct NativePad accept=native_pad_map(EOG_A,156,-156,156,156,native_setting_style(),0);
        struct NativePad back=native_pad_map(EOG_B,0,0,156,156,native_setting_style(),0);
        assert(accept.buttons==0x8000 && accept.x==80 && accept.y==-80);
        assert(back.buttons==0x4000 && !back.x && !back.y && !back.actions);
        assert(unlink(path)==0);
    }
    native_settings_init(directory);native_settings_apply_console_defaults(5);
    native_setting_set_watch(5);native_setting_set_skin(4);native_setting_set_aspect(2);
    native_settings_init(directory);assert(native_setting_watch()==5 && native_setting_skin()==4 && native_setting_aspect()==2);
    assert(!strcmp(native_colour_name(1,5),"Gold\n"));unlink(path);
    FILE *f=fopen(path,"w");assert(f);fputs("user config even if invalid\n",f);fclose(f);
    native_settings_init(directory);assert(!native_settings_apply_console_defaults(5));
    char buffer[64]={0};f=fopen(path,"r");assert(f);assert(fgets(buffer,sizeof buffer,f));fclose(f);
    assert(!strcmp(buffer,"user config even if invalid\n"));
    native_setting_set_style(9);native_setting_set_aspect(1);
    native_setting_set_crosshair(5);native_setting_set_watch(4);
    for(int i=0;i<7;i++)native_setting_set_game(i,i==6?2:1);
    native_setting_set_volume(0,12345);native_setting_set_volume(1,23456);
    assert(native_setting_audio_mixer()==1);native_setting_set_audio_mixer(0);
    native_settings_init(directory);
    assert(native_setting_style()==9 && native_setting_aspect()==1);
    assert(native_setting_crosshair()==5 && native_setting_watch()==4);
    for(int i=0;i<7;i++)assert(native_setting_game(i)==(i==6?2:1));
    assert(native_setting_volume(0)==12345 && native_setting_volume(1)==23456);
    assert(native_setting_audio_mixer()==0);
    char backup[520];snprintf(backup,sizeof backup,"%s.bak",path);
    assert(!rename(path,backup));native_settings_init(directory);
    assert(native_setting_crosshair()==5 && native_setting_watch()==4);
    assert(native_setting_volume(0)==12345 && native_setting_volume(1)==23456);
    assert(native_setting_audio_mixer()==0);
    assert(!native_settings_apply_console_defaults(0));
#ifdef NATIVE_TEST_FAT_RENAME
    assert(replace_rejected>0);
    puts("PASS SD rename-over-existing fallback preserves repeated settings updates");
#endif
    unlink(path);rmdir(directory);native_settings_init(NULL);
    assert(!native_settings_apply_console_defaults(5));
    puts("PASS first launch: all six console models, safe unknown fallback, immediate save, saved-file priority across models, corrupt-file preservation and menu A/B/Circle Pad");
}
static void check_guard_preferences(void) {
    char directory[]="/tmp/eog-guard-options-XXXXXX";assert(mkdtemp(directory));
    char path[512];snprintf(path,sizeof path,"%s/native-options.ini",directory);
    native_settings_init(directory);assert(!native_setting_guard_visibility() && native_setting_guard_ai_hz()==30);
    for(int v=0;v<2;v++)for(int i=0;i<3;i++) {
        static const int rates[]={30,15,5};native_setting_set_style(8);native_setting_set_aspect(0);
        native_setting_set_guard_visibility(v);native_setting_set_guard_ai_hz(rates[i]);native_settings_init(directory);
        assert(native_setting_guard_visibility()==v && native_setting_guard_ai_hz()==rates[i]);
    }
    native_setting_set_guard_visibility(2);native_setting_set_guard_ai_hz(0);native_setting_set_guard_ai_hz(60);
    assert(native_setting_guard_visibility()==1 && native_setting_guard_ai_hz()==5);
    FILE *f=fopen(path,"w");assert(f);fputs("EOGSETTINGS1\ncontrol_style=8\ncrosshair_colour=0\nwatch_colour=0\naspect=0\n",f);fclose(f);
    native_settings_init(directory);assert(!native_setting_guard_visibility() && native_setting_guard_ai_hz()==30 && native_setting_style()==8);
    f=fopen(path,"a");assert(f);fputs("guard_visibility=1\nguard_ai_hz=7\n",f);fclose(f);
    native_settings_init(directory);assert(!native_setting_guard_visibility() && native_setting_guard_ai_hz()==30 && native_setting_style()==-1);
    unlink(path);rmdir(directory);native_settings_init(NULL);
    puts("PASS guard options: Off/30Hz defaults, every saved toggle/rate, invalid setters/config rejection and legacy-file fallback");
}
int main(void) {
    uint64_t seen=0;
    for(int i=0;i<NATIVE_MENU_OPTIONS_COUNT;i++){int row=native_menu_option_at(i);assert(row>=0 && row<NATIVE_MENU_OPTIONS_COUNT && !(seen&(1ull<<row)));seen|=1ull<<row;}
    assert(seen==((1ull<<NATIVE_MENU_OPTIONS_COUNT)-1));
    assert(native_menu_option_at(22)==22 && native_menu_option_at(23)==23 && native_menu_option_at(24)==21);
    assert(native_menu_option_at(31)==17 && native_menu_option_at(32)==24);
    assert(native_menu_option_at(35)==12 && native_menu_option_at(36)==20);
    assert(!native_setting_guard_visibility() && native_setting_guard_ai_hz()==30);
    native_setting_set_guard_ai_hz(0);native_setting_set_guard_ai_hz(60);native_setting_set_guard_visibility(2);
    assert(!native_setting_guard_visibility() && native_setting_guard_ai_hz()==30);
    check_guard_preferences();
    check_dynamic_lod_and_vignette();
    check_first_launch_defaults();
    struct NativeFrameLimit pacing={0};
    assert(native_frame_wait(&pacing,1000000,60)==0);
    native_frame_started(&pacing,1000000,60);
    assert(native_frame_wait(&pacing,1010000,60)==6666);
    assert(native_frame_wait(&pacing,1020000,60)==0);
    assert(native_frame_wait(&pacing,1010000,30)==0);
    native_frame_started(&pacing,1010000,30);
    assert(native_frame_wait(&pacing,1020000,30)==23333);
    assert(native_frame_wait(&pacing,2000000,30)==0);
    puts("PASS 30/60 frame pacing: elapsed work subtracted, immediate rate changes, late-frame recovery");
    char directory[]="/tmp/eog-preferences-XXXXXX";assert(mkdtemp(directory));
    native_settings_init(directory);
    assert(native_setting_style()==-1 && native_setting_aspect()==-1);
    assert(!native_setting_crosshair() && !native_setting_watch() && native_setting_aa()==1);
    assert(native_setting_sensitivity()==250 && native_setting_gyro()==0 && native_setting_gyro_sensitivity()==250 && native_setting_touch_sensitivity()==250);
    assert(!native_setting_aim_movement());
    native_setting_set_aim_movement(1);native_settings_init(directory);assert(native_setting_aim_movement()==1);
    native_setting_set_aim_movement(2);assert(native_setting_aim_movement()==1);
    native_setting_set_aim_movement(0);native_settings_init(directory);assert(!native_setting_aim_movement());
    native_setting_set_style(8);native_setting_set_aspect(1);
    for(int strength=0;strength<=200;strength+=25){
        native_setting_set_stereo_strength(strength);native_settings_init(directory);
        assert(native_setting_stereo_strength()==strength);
    }
    native_setting_set_stereo_strength(100);
    for(int mode=0;mode<=2;mode++)for(int gain=25;gain<=1000;gain+=25){
        native_setting_set_gyro(mode);native_setting_set_gyro_sensitivity(gain);native_settings_init(directory);
        assert(native_setting_gyro()==mode && native_setting_gyro_sensitivity()==gain);
    }
    native_setting_set_gyro(3);native_setting_set_gyro_sensitivity(0);native_setting_set_gyro_sensitivity(101);
    assert(native_setting_gyro()==2 && native_setting_gyro_sensitivity()==1000);
    for(int gain=25;gain<=1000;gain+=25){native_setting_set_touch_sensitivity(gain);native_settings_init(directory);assert(native_setting_touch_sensitivity()==gain);}
    native_setting_set_touch_sensitivity(24);native_setting_set_touch_sensitivity(101);assert(native_setting_touch_sensitivity()==1000);
    assert(!native_setting_aim_lock() && !native_setting_stamina_sway() && native_setting_steady_scope());
    for(int space=0;space<3;space++)for(int axis=0;axis<2;axis++){
        native_setting_set_gyro_space(space);native_setting_set_gyro_axis(axis);
        native_setting_set_aim_lock(axis);native_setting_set_stamina_sway(axis);native_setting_set_steady_scope(!axis);
        native_settings_init(directory);
        assert(native_setting_gyro_space()==space && native_setting_gyro_axis()==axis);
        assert(native_setting_aim_lock()==axis && native_setting_stamina_sway()==axis && native_setting_steady_scope()==!axis);
    }
    native_setting_set_style(8);native_setting_set_crosshair(1);native_setting_set_watch(2);native_setting_set_aspect(1);native_setting_set_aa(0);native_setting_set_fps(30);native_setting_set_sensitivity(75);native_setting_set_sharpness(2);
    native_settings_init(directory);
    assert(native_setting_style()==8 && native_setting_crosshair()==1 && native_setting_watch()==2 && native_setting_aspect()==1);
    assert(native_setting_sharpness()==2);native_setting_set_sharpness(7);assert(native_setting_sharpness()==2);
    native_setting_set_sharpness(6);native_settings_init(directory);assert(native_setting_sharpness()==6);
    native_setting_set_vignette(2);native_setting_set_bloom(1);native_setting_set_grain(2);
    native_settings_init(directory);assert(native_setting_vignette()==2 && native_setting_bloom()==1 && native_setting_grain()==2);
    native_setting_set_vignette(3);native_setting_set_bloom(-1);native_setting_set_grain(3);
    assert(native_setting_vignette()==2 && native_setting_bloom()==1 && native_setting_grain()==2);
    assert(native_setting_skin()==0);native_setting_set_skin(1);native_settings_init(directory);
    assert(native_setting_skin()==1);native_setting_set_skin(5);assert(native_setting_skin()==1);
    native_setting_set_lod(3);native_setting_set_view_distance(1);native_settings_init(directory);
    assert(native_setting_lod()==3 && native_setting_view_distance()==1);
    native_setting_set_lod(5);native_setting_set_view_distance(-1);
    assert(native_setting_lod()==3 && native_setting_view_distance()==1);
    native_setting_set_lod(4);native_setting_dynamic_lod(3);native_setting_set_vignette_mode(3);
    native_settings_init(directory);assert(native_setting_lod()==4 && native_setting_lod_strength()==1 && native_setting_vignette_mode()==3);
    native_setting_dynamic_lod(2);assert(native_setting_lod()==4 && native_setting_lod_strength()==2);
    assert(native_setting_glass()==1);native_setting_set_glass(0);native_settings_init(directory);assert(native_setting_glass()==0);
    assert(native_setting_sensitivity()==75);native_setting_set_sensitivity(17);assert(native_setting_sensitivity()==75);
    assert(native_setting_fps()==30);native_setting_set_fps(45);assert(native_setting_fps()==30);
    assert(native_setting_aa()==0);native_setting_set_aa(2);assert(native_setting_aa()==0);
    native_setting_set_style(99);native_setting_set_watch(-1);assert(native_setting_style()==8 && native_setting_watch()==2);
    char path[512];snprintf(path,sizeof path,"%s/native-options.ini",directory);
    FILE *f=fopen(path,"w");assert(f);
    fputs("EOGSETTINGS1\ncontrol_style=9\ncrosshair_colour=1\nwatch_colour=3\naspect=1\nmusic_playback=1\nglass_detail=2\n",f);fclose(f);
    native_settings_init(directory);assert(native_setting_style()==9 && native_setting_watch()==3 && native_setting_aa()==1 && native_setting_fps()==60 && native_setting_sensitivity()==250 && native_setting_sharpness()==0 && !native_setting_vignette() && !native_setting_bloom() && !native_setting_grain());
    assert(native_setting_skin()==0 && native_setting_glass()==1); /* Removed music key is ignored. */
    native_setting_set_aa(0);native_settings_init(directory);assert(native_setting_aa()==0 && native_setting_style()==9);
    f=fopen(path,"w");assert(f);fputs("EOGSETTINGS1\ncontrol_style=9\ncrosshair_colour=999\nwatch_colour=3\naspect=1\n",f);fclose(f);
    native_settings_init(directory);assert(native_setting_style()==-1 && !native_setting_watch());
    f=fopen(path,"w");assert(f);fputs("EOGSETTINGS1\ncontrol_style=8\n",f);fclose(f);
    native_settings_init(directory);assert(native_setting_style()==-1);
    f=fopen(path,"w");assert(f);fputs("EOGSETTINGS1\ncontrol_style=8\ncrosshair_colour=1\nwatch_colour=2\naspect=0\nn64_aa=99\n",f);fclose(f);
    native_settings_init(directory);assert(native_setting_aa()==1 && native_setting_style()==-1);
    unlink(path);rmdir(directory);native_settings_init(NULL);
    for(unsigned mode=0;mode<65536;mode++) {
        assert(native_aa_render_mode(mode,1)==mode);
        assert(native_aa_render_mode(mode,0)==(mode&~8u));
        assert((native_aa_render_mode(mode,0)^mode)==(mode&8u));
    }
    puts("PASS N64 AA: persisted toggle, legacy preferences migration, invalid rejection and exact AA-only render-mode masking");
    assert(!strcmp(native_string(0xf000),"1.5 Vesper\n"));assert(!strcmp(native_string(0xf001),"1.6 Jinx\n"));assert(!native_string(0xffffffffu));
    char purple_directory[]="/tmp/eog-purple-XXXXXX";assert(mkdtemp(purple_directory));
    native_settings_init(purple_directory);native_setting_set_style(8);native_setting_set_aspect(0);
    native_setting_set_crosshair(6);native_setting_set_watch(6);native_setting_set_flick(1);
    native_settings_init(purple_directory);
    assert(native_setting_crosshair()==6 && native_setting_watch()==6 && native_setting_flick());
    assert(!strcmp(native_colour_name(0,6),"Purple\n") && !strcmp(native_colour_name(1,6),"Purple\n"));
    snprintf(path,sizeof path,"%s/native-options.ini",purple_directory);unlink(path);rmdir(purple_directory);
    native_settings_init(NULL);
    for(int theme=0;theme<7;++theme) {
        unsigned r=0,g=160,b=0;native_theme_rgb(theme,&r,&g,&b);
        if(!theme)assert(!r && g==160 && !b);
        if(theme==1)assert(r==160 && g==160 && b==160);
        if(theme==2)assert(r==160 && g>100 && g<120 && b>10 && b<30);
        if(theme==3)assert(r==160 && g<40 && b<40);
        if(theme==4)assert(r==40 && g==100 && b==160);
        if(theme==5)assert(r==144 && g==119 && b==40);
        if(theme==6)assert(r==120 && g==60 && b==160);
        r=255;g=0;b=0;native_theme_rgb(theme,&r,&g,&b);assert(r==255 && !g && !b);
        r=g=b=200;native_theme_rgb(theme,&r,&g,&b);assert(r==200 && g==200 && b==200);
    }
    struct NativePad p=native_pad_map(EOG_A|EOG_X|EOG_Y|EOG_R,156,156,0,0,8,1);
    assert(p.move_x==80 && p.move_y==80 && !p.x && !p.y && (p.buttons&0x4000) && (p.buttons&0x2000));
    assert(p.actions==(EOG_RELOAD|EOG_CROUCH|EOG_INTERACT));
    p=native_pad_map(0,0,0,156,-156,8,1);assert(!p.move_x && !p.move_y && p.x==80 && p.y==-80);
    p=native_pad_map(EOG_A|EOG_B,0,0,156,156,8,0);assert((p.buttons&0xc000)==0xc000 && !p.x && !p.y && !p.actions);
    p=native_pad_map(EOG_A|EOG_X|EOG_DUP|EOG_DLEFT|EOG_DRIGHT,156,0,0,0,9,1);
    assert(p.x==80 && p.y==80 && p.move_x==80 && (p.buttons&0x4000) && p.actions==(EOG_RELOAD|EOG_CROUCH|EOG_INTERACT));
    p=native_pad_map(EOG_A|EOG_Y|EOG_X|EOG_B,0,0,0,0,9,1);assert(!p.x && !p.y);
    p=native_pad_map(EOG_A|EOG_B|EOG_X|EOG_Y|EOG_L,156,-156,50,-50,0,1);
    assert((p.buttons&0xc01f)==0xc01d && p.x==80 && p.y==-80);
    assert(!native_pad_axis(11) && !native_pad_axis(-11) && native_pad_axis(1000)==80);
    puts("PASS native controls: independent axes, face-camera diamond, action separation, original mapping, menu A/B and deadzones");
    puts("PASS native preferences: restart roundtrip, invalid/truncated rejection, colour isolation and style names");
}
