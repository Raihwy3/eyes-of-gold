/* Included after the original options.c; both menus use the same settings. */
#include "native_menu_options.h"
#include "native_motion_sensor.h"
extern void native_motion_calibrate(void) __attribute__((weak));
extern int native_motion_calibration_status(void) __attribute__((weak));
extern int native_motion_calibration_progress(void) __attribute__((weak));
int native_menu_option_value(int row) {
    if(row==53)return native_setting_watch_clock();
    if(row==52)return native_setting_crosshair_opacity()<0?0:1+native_setting_crosshair_opacity()/10;
    if(row==51)return native_setting_crosshair_shape();
    if(row==44)return (native_setting_stereo_auto()-(0))/1;
    if(row==45)return (native_setting_stereo_convergence()-(10))/10;
    if(row==46)return (native_setting_stereo_comfort()-(-20))/5;
    if(row==47)return (native_setting_stereo_hud_depth()-(0))/5;
    if(row==48)return (native_setting_stereo_crosshair_hud()-(0))/1;
    if(row==49)return (native_setting_stereo_ghost_contrast()-(50))/5;
    if(row==50)return (native_setting_stereo_black_floor()-(0))/1;
    if(row==41)return native_setting_gyro_filter();
    if(row==42)return (native_setting_vertical_sensitivity()-25)/25;
    if(row==43)return native_setting_always_crosshair();
    if(row==40)return native_setting_stereo_strength()/25;
    if(row==39)return native_setting_aim_movement();
    if(row==33)return native_setting_gyro_space();
    if(row==34)return native_setting_gyro_axis();
    if(row==35)return 0;
    if(row==36)return native_setting_aim_lock();
    if(row==37)return native_setting_stamina_sway();
    if(row==38)return native_setting_steady_scope();
    if(row==32)return native_setting_flick();
    if(row==31)return (native_setting_touch_sensitivity()-25)/25;
    if(row==29)return native_setting_gyro();
    if(row==30)return (native_setting_gyro_sensitivity()-25)/25;
    if(row==27)return native_setting_guard_visibility();
    if(row==28){int hz=native_setting_guard_ai_hz();return hz==5?2:hz==15?1:0;}
    if(row==26)return native_setting_unlock_all();
    if(row==25)return native_setting_audio_mixer();
    if(row==0){int s=native_setting_style();if(s<0)s=g_CurrentPlayer?g_CurrentPlayer->cur_player_control_type_0:0;return s>=8?s-4:s<4?s:0;}
    if(row==1 || row==2){int v=native_setting_volume(row==2);if(v<0)v=row==1?get_mTrack2Vol():call_sndGetSfxSlotFirstNaturalVolume();return (v*100+16383)/32767;}
    if(row>=3 && row<=10){int v=native_setting_game(row-3);return v>=0?v:game_options_entries[row-3].current_value;}
    if(row==24)return native_setting_vignette_mode();
    if(row==22)return native_setting_lod();
    if(row==23)return native_setting_view_distance();
    if(row==20)return native_setting_skin();
    if(row==21)return native_setting_glass();
    if(row==17)return native_setting_vignette();
    if(row==19)return native_setting_grain();
    if(row==18)return native_setting_bloom();
    if(row==16)return native_setting_sharpness();
    if(row==15)return (native_setting_sensitivity()-25)/25;
    if(row==14)return native_setting_fps()==60;
    return row==11?native_setting_crosshair():row==12?native_setting_watch():native_setting_aa();
}
void native_menu_option_change(int row,int direction) {
    if(row<0 || row>=NATIVE_MENU_OPTIONS_COUNT)return;
    if(row==53){native_setting_set_watch_clock(!native_setting_watch_clock());return;}
    if(row==52){int index=(native_menu_option_value(row)+direction+12)%12;native_setting_set_crosshair_opacity(index?10*(index-1):-1);return;}
    if(row==51){native_setting_set_crosshair_shape((native_setting_crosshair_shape()+direction+8)%8);return;}
    if(row==44){native_setting_set_stereo_auto(0+1*((native_menu_option_value(row)+direction+2)%2));return;}
    if(row==45){native_setting_set_stereo_convergence(10+10*((native_menu_option_value(row)+direction+20)%20));return;}
    if(row==46){native_setting_set_stereo_comfort(-20+5*((native_menu_option_value(row)+direction+11)%11));return;}
    if(row==47){native_setting_set_stereo_hud_depth(0+5*((native_menu_option_value(row)+direction+21)%21));return;}
    if(row==48){native_setting_set_stereo_crosshair_hud(0+1*((native_menu_option_value(row)+direction+2)%2));return;}
    if(row==49){native_setting_set_stereo_ghost_contrast(50+5*((native_menu_option_value(row)+direction+11)%11));return;}
    if(row==50){native_setting_set_stereo_black_floor(0+1*((native_menu_option_value(row)+direction+21)%21));return;}
    if(row==41){native_setting_set_gyro_filter(!native_setting_gyro_filter());return;}
    if(row==42){native_setting_set_vertical_sensitivity(25+25*((native_menu_option_value(row)+direction+40)%40));return;}
    if(row==43){native_setting_set_always_crosshair(!native_setting_always_crosshair());return;}
    if(row==40){native_setting_set_stereo_strength(25*((native_menu_option_value(row)+direction+9)%9));return;}
    if(row==39){native_setting_set_aim_movement(!native_setting_aim_movement());return;}
    if(row==33){native_setting_set_gyro_space((native_setting_gyro_space()+direction+3)%3);return;}
    if(row==34){native_setting_set_gyro_axis(!native_setting_gyro_axis());return;}
    if(row==35){if(native_motion_calibrate)native_motion_calibrate();return;}
    if(row==36){native_setting_set_aim_lock(!native_setting_aim_lock());return;}
    if(row==37){native_setting_set_stamina_sway(!native_setting_stamina_sway());return;}
    if(row==38){native_setting_set_steady_scope(!native_setting_steady_scope());return;}
    if(row==32){native_setting_set_flick(!native_setting_flick());return;}
    if(row==31){native_setting_set_touch_sensitivity(25+25*((native_menu_option_value(row)+direction+40)%40));return;}
    if(row==29){native_setting_set_gyro((native_setting_gyro()+direction+3)%3);return;}
    if(row==30){native_setting_set_gyro_sensitivity(25+25*((native_menu_option_value(row)+direction+16)%16));return;}
    if(row==27){native_setting_set_guard_visibility(!native_setting_guard_visibility());return;}
    if(row==28){static const int rates[]={30,15,5};native_setting_set_guard_ai_hz(rates[(native_menu_option_value(row)+direction+3)%3]);return;}
    if(row==26){native_setting_set_unlock_all(!native_setting_unlock_all());return;}
    if(row==25){native_setting_set_audio_mixer((native_setting_audio_mixer()+direction+4)%4);return;}
    int value=native_menu_option_value(row);
    if(row==1 || row==2){value+=direction*5;if(value<0)value=0;if(value>100)value=100;int volume=(value*32767+50)/100;native_setting_set_volume(row==2,volume);if(row==1)set_mTrack2Vol(volume);else sndApplyVolumeAllSfxSlot(volume);return;}
    int count=row==22?5:row>=23?4:row==20?5:row==21?2:row==0?6:row==9 || row==10?3:row==11?7:row==12?7:row==15?40:row==16?7:row>=17?3:2;
    value=(value+direction+count)%count;
    if(row==0){int style=value>=4?value+4:value;native_setting_set_style(style);if(g_CurrentPlayer)cur_player_set_control_type(style);}
    else if(row<=10){native_setting_set_game(row-3,value);game_options_entries[row-3].current_value=value;}
    else if(row==11)native_setting_set_crosshair(value);
    else if(row==12)native_setting_set_watch(value);
    else if(row==13)native_setting_set_aa(value);
    else if(row==14)native_setting_set_fps(value?60:30);
    else if(row==15)native_setting_set_sensitivity(25+25*value);
    else if(row==16)native_setting_set_sharpness(value);
    else if(row==17)native_setting_set_vignette(value);
    else if(row==18)native_setting_set_bloom(value);
    else if(row==19)native_setting_set_grain(value);
    else if(row==20)native_setting_set_skin(value);
    else if(row==21)native_setting_set_glass(value);
    else if(row==22)native_setting_set_lod(value);
    else if(row==23)native_setting_set_view_distance(value);
    else native_setting_set_vignette_mode(value);
}
const char *native_menu_option_label(int row) {
    static const char *const labels[]={0,"Music volume\n","Effects volume\n",0,0,0,0,0,0,0,"Aspect ratio\n","Crosshair colour\n","Watch colour\n","Supersampling\n","FPS limit\n","Look horizontal\n","Image filter\n","Vignette\n","Bloom\n","Film grain\n","Watch skin\n","Glass detail\n","LOD strength\n","View distance\n","Vignette when\n","Mixer (restart)\n","Unlock everything\n","Guard LOS (exp)\n","Guard AI rate\n","Gyro look\n","Gyro sensitivity\n","Touch sensitivity\n","Flick stick (exp)\n","Gyro space\n","Local turn axis\n","Calibrate gyro\n","Aim lock\n","Stamina sway\n","Steady scope\n","Aim movement\n","3D strength\n","Gyro filter\n","Look vertical\n","Always crosshair\n","Auto convergence\n","Convergence\n","Pop-out limit\n","HUD depth\n","Crosshair depth\n","Ghost contrast\n","Black floor\n","Crosshair style\n","Crosshair opacity\n","Console watch time\n"};
    if(row==0)return (const char *)langGet(getStringID(LOPTIONS,OPTION_STR_32_CONTROLSTYLE_LF));
    if(row>=3 && row<=9)return (const char *)langGet(game_options_entries[row-3].text[0]);
    return row>=0 && row<NATIVE_MENU_OPTIONS_COUNT?labels[row]:"\n";
}
const char *native_menu_option_text(int row,char *buffer,unsigned size) {
    if(row<0 || row>=NATIVE_MENU_OPTIONS_COUNT)return "\n";
    if(row==53)return native_setting_watch_clock()?"On\n":"Off\n";
    if(row==52){if(native_setting_crosshair_opacity()<0)return "Original\n";snprintf(buffer,size,"%d%%\n",native_setting_crosshair_opacity());return buffer;}
    if(row==51){static const char *const shapes[]={"Original\n","Dot\n","Cross\n","Cross + dot\n","Circle\n","Circle + dot\n","T-shape\n","Diamond\n"};return shapes[native_setting_crosshair_shape()];}
    static const char *const styles[]={"1.5 Vesper\n","1.6 Jinx\n"};
    if(row==44)return native_setting_stereo_auto()?"On\n":"Off\n";
    if(row==45){snprintf(buffer,size,"%d%%\n",native_setting_stereo_convergence());return buffer;}
    if(row==46){int value=native_setting_stereo_comfort();snprintf(buffer,size,"%s%d.%d%%\n",value<0?"-":"",(value<0?-value:value)/10,(value<0?-value:value)%10);return buffer;}
    if(row==47){snprintf(buffer,size,"%d%%\n",native_setting_stereo_hud_depth());return buffer;}
    if(row==48)return native_setting_stereo_crosshair_hud()?"HUD\n":"Dynamic\n";
    if(row==49){snprintf(buffer,size,"%d%%\n",native_setting_stereo_ghost_contrast());return buffer;}
    if(row==50){snprintf(buffer,size,"%d%%\n",native_setting_stereo_black_floor());return buffer;}
    if(row==42){snprintf(buffer,size,"%d%%\n",native_setting_vertical_sensitivity()*2/5);return buffer;}
    if(row==40){snprintf(buffer,size,"%d%%\n",native_setting_stereo_strength());return buffer;}
    if(row==39)return native_setting_aim_movement()?"Classic\n":"Free\n";
    if(row==33){static const char *const spaces[]={"Local\n","World\n","Player\n"};return spaces[native_setting_gyro_space()];}
    if(row==34)return native_setting_gyro_space()?"Gravity\n":native_setting_gyro_axis()?"Roll\n":"Yaw\n";
    if(row==35){
        int state=native_motion_calibration_status?native_motion_calibration_status():0;
        if(state==2){int percent=native_motion_calibration_progress?native_motion_calibration_progress():0;snprintf(buffer,size,"Still %d%%\n",percent);return buffer;}
        return state==1?"Keep still\n":state==-1?"Calibrated\n":state==-2?"Try again\n":"Start\n";
    }
    if(row>=36)return native_menu_option_value(row)?"On\n":"Off\n";
    if(row==32)return native_setting_flick()?"On\n":"Off\n";
    if(row==31){snprintf(buffer,size,"%d%%\n",native_setting_touch_sensitivity()*2/5);return buffer;}
    if(row==29){static const char *const modes[]={"Off\n","Aim only\n","Look + aim\n"};return modes[native_setting_gyro()];}
    if(row==30){snprintf(buffer,size,"%d%%\n",native_setting_gyro_sensitivity());return buffer;}
    if(row==27)return native_setting_guard_visibility()?"On\n":"Off\n";
    if(row==28){snprintf(buffer,size,"%d Hz\n",native_setting_guard_ai_hz());return buffer;}
    if(row==26)return native_setting_unlock_all()?"On\n":"Off\n";
    if(row==25){static const char *const modes[]={"CPU\n","NDSP\n","OpenDSP\n","OpenDSP FX\n"};return modes[native_setting_audio_mixer()];}
    int value=native_menu_option_value(row);
    if(row==0)return value<4?(const char *)langGet(game_control_styles[value*10]):styles[value-4];
    if(row==24){static const char *const modes[]={"Always\n","Crouch\n","Zoom\n","Crouch/Zoom\n"};return modes[value];}
    if(row==22){static const char *const levels[]={"Original\n","Balanced\n","Strong\n","Maximum\n","Dynamic\n"};return levels[value];}
    if(row==23){static const char *const levels[]={"Original\n","Far\n","Medium\n","Near\n"};return levels[value];}
    if(row==20){static const char *const skins[]={"Original\n","Field\n","Dress\n","Tactical\n","Gold\n"};return skins[value];}
    if(row==21){static const char *const glass[]={"Original\n","Simple\n"};return glass[value];}
    if(row>=17){static const char *const levels[]={"Off\n","Low\n","High\n"};return levels[value];}
    if(row==16){static const char *const levels[]={"Off\n","Sharp low\n","Sharp medium\n","Sharp high\n","Smooth low\n","Smooth medium\n","Smooth high\n"};return levels[value];}
    if(row==15){snprintf(buffer,size,"%d%%\n",(25+25*value)*2/5);return buffer;}
    if(row==14)return value?"60 FPS\n":"30 FPS\n";
    if(row==1 || row==2){snprintf(buffer,size,"%d%%\n",value);return buffer;}
    if(row>=3 && row<=9)return (const char *)langGet(game_options_entries[row-3].text[value+1]);
    if(row==10)return native_aspect_label(value);
    if(row==11 || row==12)return native_colour_name(row==12,value);
    return value?"On\n":"Off\n";
}
void native_menu_options_restore_audio(void) {
    int music=native_setting_volume(0),effects=native_setting_volume(1);
    if(music>=0)set_mTrack2Vol(music);
    if(effects>=0)sndApplyVolumeAllSfxSlot(effects);
}
