/* Actual patched frontend arena availability and virtual slot policy. */
#include "front.c"
#include <stdlib.h>
#include <stdio.h>
int main(void){
 if(native_preview_controller_count()!=2)abort();
 gamemode=GAMEMODE_MULTI;selected_num_players=2;
 if(get_selected_num_players()!=2)abort();
 for(int i=MP_STAGE_RANDOM;i<MP_STAGE_SELECTED_MAX;i++)if(!check_if_mp_stage_unlocked(i))abort();
 if(check_if_mp_stage_unlocked(-1) || check_if_mp_stage_unlocked(MP_STAGE_SELECTED_MAX))abort();
 gamemode=GAMEMODE_SOLO;if(get_selected_num_players()!=1)abort();
 for(int i=0;i<MAX_PLAYER_COUNT;i++)player_has_selected_char[i]=FALSE;
 native_preview_ready_idle();
 if(player_has_selected_char[0] || !player_has_selected_char[1] || player_has_selected_char[2] || player_has_selected_char[3])abort();
 puts("PASS actual multiplayer frontend: two engine slots, all eleven arenas plus Random, safe invalid indices, unchanged solo count and only idle P2 marked ready");
}
