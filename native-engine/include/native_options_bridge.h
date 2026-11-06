/* Included after the original options.c; both menus use the same settings. */
#include "native_menu_options.h"
int native_menu_option_value(int row) {
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
    if(row==32){native_setting_set_flick(!native_setting_flick());return;}
    if(row==31){native_setting_set_touch_sensitivity(25+25*((native_menu_option_value(row)+direction+8)%8));return;}
    if(row==29){native_setting_set_gyro((native_setting_gyro()+direction+3)%3);return;}
    if(row==30){native_setting_set_gyro_sensitivity(25+25*((native_menu_option_value(row)+direction+8)%8));return;}
    if(row==27){native_setting_set_guard_visibility(!native_setting_guard_visibility());return;}
    if(row==28){static const int rates[]={30,15,5};native_setting_set_guard_ai_hz(rates[(native_menu_option_value(row)+direction+3)%3]);return;}
    if(row==26){native_setting_set_unlock_all(!native_setting_unlock_all());return;}
    if(row==25){native_setting_set_audio_mixer(!native_setting_audio_mixer());return;}
    int value=native_menu_option_value(row);
    if(row==1 || row==2){value+=direction*5;if(value<0)value=0;if(value>100)value=100;int volume=(value*32767+50)/100;native_setting_set_volume(row==2,volume);if(row==1)set_mTrack2Vol(volume);else sndApplyVolumeAllSfxSlot(volume);return;}
    int count=row==22?5:row>=23?4:row==20?5:row==21?2:row==0?6:row==9 || row==10?3:row==11?7:row==12?7:row==15?8:row==16?7:row>=17?3:2;
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
    static const char *const labels[]={0,"Music volume\n","Effects volume\n",0,0,0,0,0,0,0,"Aspect ratio\n","Crosshair\n","Watch colour\n","Supersampling\n","FPS limit\n","Look sensitivity\n","Image filter\n","Vignette\n","Bloom\n","Film grain\n","Watch skin\n","Glass detail\n","LOD strength\n","View distance\n","Vignette when\n","Mixer (restart)\n","Unlock everything\n","Guard LOS (exp)\n","Guard AI rate\n","Gyro look\n","Gyro sensitivity\n","Touch sensitivity\n","Flick stick (exp)\n"};
    if(row==0)return (const char *)langGet(getStringID(LOPTIONS,OPTION_STR_32_CONTROLSTYLE_LF));
    if(row>=3 && row<=9)return (const char *)langGet(game_options_entries[row-3].text[0]);
    return row>=0 && row<NATIVE_MENU_OPTIONS_COUNT?labels[row]:"\n";
}
const char *native_menu_option_text(int row,char *buffer,unsigned size) {
    if(row<0 || row>=NATIVE_MENU_OPTIONS_COUNT)return "\n";
    static const char *const styles[]={"1.5 Vesper\n","1.6 Jinx\n"};
    if(row==32)return native_setting_flick()?"On\n":"Off\n";
    if(row==31){snprintf(buffer,size,"%d%%\n",native_setting_touch_sensitivity());return buffer;}
    if(row==29){static const char *const modes[]={"Off\n","Aim only\n","Look + aim\n"};return modes[native_setting_gyro()];}
    if(row==30){snprintf(buffer,size,"%d%%\n",native_setting_gyro_sensitivity());return buffer;}
    if(row==27)return native_setting_guard_visibility()?"On\n":"Off\n";
    if(row==28){snprintf(buffer,size,"%d Hz\n",native_setting_guard_ai_hz());return buffer;}
    if(row==26)return native_setting_unlock_all()?"On\n":"Off\n";
    if(row==25)return native_setting_audio_mixer()?"DSP\n":"CPU\n";
    int value=native_menu_option_value(row);
    if(row==0)return value<4?(const char *)langGet(game_control_styles[value*10]):styles[value-4];
    if(row==24){static const char *const modes[]={"Always\n","Crouch\n","Zoom\n","Crouch/Zoom\n"};return modes[value];}
    if(row==22){static const char *const levels[]={"Original\n","Balanced\n","Strong\n","Maximum\n","Dynamic\n"};return levels[value];}
    if(row==23){static const char *const levels[]={"Original\n","Far\n","Medium\n","Near\n"};return levels[value];}
    if(row==20){static const char *const skins[]={"Original\n","Field\n","Dress\n","Tactical\n","Gold\n"};return skins[value];}
    if(row==21){static const char *const glass[]={"Original\n","Simple\n"};return glass[value];}
    if(row>=17){static const char *const levels[]={"Off\n","Low\n","High\n"};return levels[value];}
    if(row==16){static const char *const levels[]={"Off\n","Sharp low\n","Sharp medium\n","Sharp high\n","Smooth low\n","Smooth medium\n","Smooth high\n"};return levels[value];}
    if(row==15){snprintf(buffer,size,"%d%%\n",25+25*value);return buffer;}
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
