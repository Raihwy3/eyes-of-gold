/* Actual patched frontend arena availability and virtual slot policy. */
#include "front.c"
#include <stdlib.h>
#include <stdio.h>
int main(void){
 if(native_preview_controller_count()!=4)abort();
 gamemode=GAMEMODE_MULTI;
 selected_num_players=2;if(get_selected_num_players()!=2)abort();
 selected_num_players=3;if(get_selected_num_players()!=3)abort();
 selected_num_players=4;if(get_selected_num_players()!=4)abort();
 for(int i=MP_STAGE_RANDOM;i<MP_STAGE_SELECTED_MAX;i++)if(!check_if_mp_stage_unlocked(i))abort();
 if(check_if_mp_stage_unlocked(-1) || check_if_mp_stage_unlocked(MP_STAGE_SELECTED_MAX))abort();
 gamemode=GAMEMODE_SOLO;if(get_selected_num_players()!=1)abort();
 for(int i=0;i<MAX_PLAYER_COUNT;i++)player_has_selected_char[i]=FALSE;
 native_preview_ready_idle();
 if(player_has_selected_char[0] || !player_has_selected_char[1] || !player_has_selected_char[2] || !player_has_selected_char[3])abort();
 if(strcmp(native_mp_players_label(),"Players")!=0)abort();
 if(strcmp(native_mp_players_value_text(2),"2")!=0)abort();
 if(strcmp(native_mp_players_value_text(3),"3")!=0)abort();
 if(strcmp(native_mp_players_value_text(4),"4")!=0)abort();
 if(strcmp(native_mp_bot_label(),"Bots")!=0)abort();
 if(native_mp_bot_enabled()!=0 || native_mp_bot_fullscreen()!=0 || strcmp(native_mp_bot_value_text(),"Off")!=0)abort();
 native_mp_bot_toggle();
 if(native_mp_bot_enabled()!=1 || native_mp_bot_fullscreen()!=0 || strcmp(native_mp_bot_value_text(),"On")!=0)abort();
 native_mp_bot_toggle();
 if(native_mp_bot_enabled()!=1 || native_mp_bot_fullscreen()!=1 || strcmp(native_mp_bot_value_text(),"Full P1")!=0)abort();
 native_mp_bot_toggle();
 if(native_mp_bot_enabled()!=0 || native_mp_bot_fullscreen()!=0 || strcmp(native_mp_bot_value_text(),"Off")!=0)abort();
 puts("PASS actual multiplayer frontend: 2-4 player slots, all eleven arenas plus Random, safe invalid indices, unchanged solo count, 3-state Bots toggle (Off/On/Full P1), and idle P2-P4 marked ready");
}
