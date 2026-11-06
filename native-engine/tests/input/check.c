/* Run the actual HID sampler with deterministic polling between game ticks. */
#include <3ds.h>
#include "native_input.h"
#include "native_pad.h"
#include "native_settings.h"
#include <assert.h>
#include <stdio.h>
angularRate test_gyro_rate;
static unsigned held;
static int gameplay=1;
void hidScanInput(void){}
u32 hidKeysHeld(void){return held;}
void hidCircleRead(circlePosition *p){p->dx=50;p->dy=-50;}
void hidCstickRead(circlePosition *p){p->dx=p->dy=0;}
int native_game_input_gameplay(void){return gameplay;}
extern void inputUpdate(void);
extern unsigned inputComputePad(int,signed char *,signed char *);
int main(void) {
 native_settings_init(NULL);
 for(int style=8;style<=9;style++) {
  native_setting_set_style(style);native_input_init();
  unsigned key=style==8?EOG_A:EOG_DRIGHT;
  held=key;inputUpdate();inputUpdate(); // Several HID polls, no game tick yet.
  held=0;inputUpdate(); // Release before gameplay consumes the input.
  assert(!(inputComputePad(0,NULL,NULL)&0x4000));
  int x,y;unsigned actions;
  native_input_take_motion(&x,&y,&actions);assert(actions==EOG_INTERACT && x>0 && y<0);
  native_input_take_motion(&x,&y,&actions);assert(actions==0);
  held=key;inputUpdate();native_input_take_motion(&x,&y,&actions);assert(actions==EOG_INTERACT);
  inputUpdate();native_input_take_motion(&x,&y,&actions);assert(!actions); // Holding never repeats.
  held=0;inputUpdate();held=key;inputUpdate();
  gameplay=0;inputUpdate();native_input_take_motion(&x,&y,&actions);assert(!actions);
  held=0;inputUpdate();gameplay=1;inputUpdate();native_input_take_motion(&x,&y,&actions);assert(!actions);
 }
 native_input_init();gameplay=1;held=EOG_SELECT;inputUpdate();
 assert(native_input_menu_buttons()==EOG_SELECT);
 held=0;inputUpdate();gameplay=0;held=EOG_SELECT;inputUpdate();
 assert(native_input_menu_buttons()==EOG_SELECT);
 held=EOG_SELECT|EOG_START;inputUpdate();assert(native_input_exit_requested());
 held=EOG_Y|EOG_A;inputUpdate();assert(native_input_menu_buttons()==(EOG_Y|EOG_A));
 native_input_init();assert(native_input_menu_buttons()==0);
 held=0;inputUpdate();native_input_press(0x8000);
 assert(inputComputePad(0,NULL,NULL)&0x8000);assert(!(inputComputePad(0,NULL,NULL)&0x8000));
 native_input_press(0x1000);native_input_init();assert(!(inputComputePad(0,NULL,NULL)&0x1000));
 held=0;gameplay=1;test_gyro_rate.z=1438;test_gyro_rate.x=719;
 native_setting_set_gyro(2);float gy=0,gp=0,total_y=0,total_p=0;
 for(int i=0;i<60;i++){inputUpdate();native_input_take_gyro(&gy,&gp);total_y+=gy;total_p+=gp;}
 assert(total_y>95 && total_y<105 && total_p>45 && total_p<55);
 native_input_take_gyro(&gy,&gp);assert(gy==0 && gp==0);
 native_setting_set_gyro(1);inputUpdate();native_input_take_gyro(&gy,&gp);assert(gy==0 && gp==0);
 held=EOG_L;inputUpdate();native_input_take_gyro(&gy,&gp);assert(gy>0 && gp>0);
 gameplay=0;inputUpdate();native_input_take_gyro(&gy,&gp);assert(gy==0 && gp==0);
 native_input_suspend();native_input_take_gyro(&gy,&gp);assert(gy==0 && gp==0);
 native_setting_set_gyro(0);
 puts("PASS HID gyro integration: degrees/sec, consume-once, aim-only, menus and suspend discard motion");
 puts("PASS SELECT no longer changes weapon rendering; menu input and exit chord preserved");
 puts("PASS actual HID sampler: Vesper A/Jinx D-right taps survive intervening polls/releases, fire once, and clear on menu entry");
}
