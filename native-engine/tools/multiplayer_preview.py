"""Use the original local two-player match with only physical pad 0 present."""
import effect_lod

def front(text):
    # Only the frontend sees two available slots. Real controller input remains
    # one connected pad, and the original second player's input stays neutral.
    text=text.replace('joyGetControllerCount()','native_preview_controller_count()')
    text='static int native_preview_controller_count(void){return 2;}\n'+text
    signature='s32 check_if_mp_stage_unlocked(s32 stage)'
    # All original selectable arenas are available for this local preview.
    text=effect_lod.prefix(text,signature,'    if(stage<MP_STAGE_RANDOM || stage>=MP_STAGE_SELECTED_MAX)return FALSE;\n    return TRUE;')
    text='static void native_preview_ready_idle(void);\n'+text
    for signature in ('void interface_menu0F_mpcharsel(void)','void interface_menu10_mphandicap(void)','void interface_menu11_mpcontrols(void)'):
        text=effect_lod.prefix(text,signature,'    native_preview_ready_idle();')
    text+='\nstatic void native_preview_ready_idle(void){player_has_selected_char[PLAYER_2]=TRUE;}\n'
    return text
