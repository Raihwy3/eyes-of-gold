"""Shared main-menu/watch choices, with native persistence over legacy restores."""
def apply(text):
    for index,name in enumerate(('LOOK','AUTOAIM','AIM','SIGHT','LOOKAHEAD','AMMODISPLAY','SCREEN','RATIO')):
        import re
        pattern=r'(    game_options_entries\[PLAYER_OPTION_'+name+r'\]\.current_value = )([^;]+);'
        text,n=re.subn(pattern,lambda m:m[1]+f'(native_setting_game({index})>=0?native_setting_game({index}):'+m[2]+');',text)
        assert n==1,(name,n)
        needle=f'return game_options_entries[PLAYER_OPTION_{name}].current_value;'
        assert text.count(needle)==1
        text=text.replace(needle,f'return native_setting_game({index})>=0?native_setting_game({index}):game_options_entries[PLAYER_OPTION_{name}].current_value;')
    needle='    *param_1 = param_2;'
    assert text.count(needle)==1
    text=text.replace(needle,'    for(int i=0;i<8;i++)if(param_1==&game_options_entries[i].current_value)native_setting_set_game(i,param_2);\n'+needle)
    needle='    game_options_entries[PLAYER_OPTION_RATIO].current_value=native_setting_aspect();'
    # Existing aspect restore has its condition on the same line.
    needle='    if(native_setting_aspect()>=0)game_options_entries[PLAYER_OPTION_RATIO].current_value=native_setting_aspect();'
    assert text.count(needle)==1
    text=text.replace(needle,'    for(int i=0;i<8;i++)if(native_setting_game(i)>=0)game_options_entries[i].current_value=native_setting_game(i);')
    needle='  return mTrack2Vol;'
    assert text.count(needle)==1
    text=text.replace(needle,'  int saved=native_setting_volume(0);return saved>=0?saved:mTrack2Vol;')
    needle='    mTrack2Vol = param_1;'
    assert text.count(needle)==1
    text=text.replace(needle,'    if(native_setting_volume(0)>=0)param_1=native_setting_volume(0);\n'+needle)
    needle='        watch_adjust_volume_slider(&volume);'
    assert text.count(needle)==2
    text=text.replace(needle,needle+'\n        native_setting_set_volume(1,volume);',1)
    pos=text.rindex(needle)+len(needle)
    text=text[:pos]+'\n        native_setting_set_volume(0,volume);'+text[pos:]
    needle='    volume = sndGetSfxSlotFirstNaturalVolume();'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'\n    if(native_setting_volume(1)>=0)volume=native_setting_volume(1);')
    needle='    sndApplyVolumeAllSfxSlot(arg0);'
    assert text.count(needle)==1
    text=text.replace(needle,'    if(native_setting_volume(1)>=0)arg0=native_setting_volume(1);\n'+needle)
    return text+'\n#include "native_options_bridge.h"\n'
