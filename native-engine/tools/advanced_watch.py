"""Keep the original watch model/theme while providing uncluttered subpages."""
def apply(text):
    text='#include <ultra64.h>\n#include "native_menu_options.h"\nstatic void native_watch_options_input(void);\nstatic void native_watch_options_reset(void);\nstatic Gfx *native_watch_options_draw(Gfx *,Mtx *);\n'+text
    # Physical tab changes must reset the custom selection/edit latch too.
    needle='void sub_GAME_7F0A6A80(void)\n{'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'''
    static int native_previous_watch_page=-1;
    if(native_previous_watch_page!=watch_screen_index){
        if(watch_screen_index==WATCH_INDEX_GAME_OPTIONS)native_watch_options_reset();
        native_previous_watch_page=watch_screen_index;
    }
''')
    # Native input owns edits; rendering the original sliders must not read
    # held shoulders/stick again and change a value a second time.
    needle='void watch_adjust_volume_slider(u16* outVolume) {'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'\n    if(watch_screen_index==WATCH_INDEX_GAME_OPTIONS)return;')
    # Only replace the game-options navigation arm, not the renderer arm.
    start=text.index('void sub_GAME_7F0A6A80(void)')
    a=text.index('        case WATCH_INDEX_GAME_OPTIONS:',start)
    b=text.index('        case WATCH_INDEX_MISSION_BRIEFING:',a)
    text=text[:a]+'        case WATCH_INDEX_GAME_OPTIONS:\n            native_watch_options_input();\n            break;\n\n'+text[b:]
    signature='Gfx *draw_watch_game_options_page(Gfx *gdl, Mtx *param_2) {'
    a=text.index(signature);pos=a+len(signature);depth=1;end=pos
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    text=text[:a]+signature+' return native_watch_options_draw(gdl,param_2); }'+text[end:]
    needle='(watch_screen_index != WATCH_INDEX_INVENTORY) && (joyGetButtonsPressedThisFrame'
    assert text.count(needle)==1
    text=text.replace(needle,'(watch_screen_index != WATCH_INDEX_INVENTORY && watch_screen_index != WATCH_INDEX_GAME_OPTIONS) && (joyGetButtonsPressedThisFrame')
    needle='void init_watch_at_start_of_stage(int stage)\n{'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'\n    native_watch_options_reset();')
    return text+'\n#include "native_watch_options.h"\n'
