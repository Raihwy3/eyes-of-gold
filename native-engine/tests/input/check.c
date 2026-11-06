/* Run the actual HID sampler with deterministic polling between game ticks. */
#include <3ds.h>
#include "native_input.h"
#include "native_pad.h"
#include "native_settings.h"
#include "native_flick.h"
#include <math.h>
#include <assert.h>
#include <stdio.h>
angularRate test_gyro_rate;
static unsigned held;
static int gameplay=1,rx,ry;
void hidScanInput(void){}
u32 hidKeysHeld(void){return held;}
void hidCircleRead(circlePosition *p){p->dx=50;p->dy=-50;}
void hidCstickRead(circlePosition *p){p->dx=rx;p->dy=ry;}
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
 /* Real sampler: suppress conventional C-stick look, retain left movement,
  * deliver each turn once and reset on menus/suspend. */
 native_setting_set_style(8);native_setting_set_flick(1);native_input_init();
 gameplay=1;held=0;rx=ry=0;inputUpdate();rx=156;
 float turn,total=0; signed char px,py;
 for(int i=0;i<9;i++){inputUpdate();native_input_take_flick(&turn);total+=turn;}
 assert(fabsf(total-90)<.01f);
 inputComputePad(0,&px,&py);assert(px==0 && py==0);
 int mx,my;unsigned ma;native_input_take_motion(&mx,&my,&ma);assert(mx>0 && my<0);
 inputUpdate();native_input_take_flick(&turn);assert(turn==0);
 native_input_take_flick(&turn);assert(turn==0);
 ry=-156;rx=0;inputUpdate();native_input_take_flick(&turn);assert(fabsf(turn-90)<.01f);
 gameplay=0;inputUpdate();native_input_take_flick(&turn);assert(turn==0);
 gameplay=1;inputUpdate();native_input_take_flick(&turn);assert(turn==0); // Held through menu: centre first.
 rx=ry=0;inputUpdate();rx=-156;inputUpdate();native_input_suspend();native_input_take_flick(&turn);assert(turn==0);
 native_setting_set_flick(0);native_input_init();rx=156;ry=0;inputUpdate();inputComputePad(0,&px,&py);assert(px==80 && py==0);
 rx=ry=0;
 for(int hz=30;hz<=60;hz+=30)for(int dir=-1;dir<=1;dir+=2) {
   struct NativeFlick f={0};uint64_t now=1000000;
   native_flick_sample(&f,now,0,0,1);
   for(int i=0;i<12;i++){now+=1000000/hz;native_flick_sample(&f,now,dir*156,0,1);}
   assert(fabsf(f.pending-dir*90)<.01f && f.remaining_us==0);
   native_flick_sample(&f,now+400000,0,-156,1);assert(f.pending==0 && !f.active);
 }
 /* Uneven C-stick pressure must work in every direction. A pressure dip
  * while holding left must not re-arm and repeatedly add another 90 degrees. */
 const int directions[4][2]={{-55,0},{55,0},{0,55},{0,-55}};
 const float expected_turn[4]={-90,90,0,180};
 for(int d=0;d<4;d++) {
   struct NativeFlick f={0};uint64_t t=1000000;native_flick_sample(&f,t,0,0,1);
   for(int i=0;i<12;i++){t+=16667;native_flick_sample(&f,t,directions[d][0],directions[d][1],1);}
   assert(fabsf(f.pending-expected_turn[d])<.02f);
   for(int i=0;i<12;i++){t+=16667;native_flick_sample(&f,t,directions[d][0]/2,directions[d][1]/2,1);}
   assert(fabsf(f.pending-expected_turn[d])<.02f);
 }
 struct NativeFlick slow={0};native_flick_sample(&slow,1000000,0,0,1);
 native_flick_sample(&slow,1016667,-60,0,1);native_flick_sample(&slow,1136667,-60,0,1);
 assert(fabsf(slow.pending+90)<.02f); // 120ms game frame completes, without re-flicking.
 native_flick_sample(&slow,1153334,0,0,1);native_flick_sample(&slow,1170001,0,-60,1);
 native_flick_sample(&slow,1290001,0,-60,1);assert(fabsf(slow.pending-90)<.02f);
 struct NativeFlick wrap={0};native_flick_sample(&wrap,1000000,0,0,1);
 native_flick_sample(&wrap,1010000,1,-156,1);native_flick_sample(&wrap,1050000,1,-156,1);
 wrap.pending=wrap.remaining=0;wrap.remaining_us=0;
 native_flick_sample(&wrap,1060000,-1,-156,1);assert(wrap.pending==0); // Back-axis jitter is not rotation.
 native_flick_sample(&wrap,1070000,0,0,0);assert(wrap.pending==0);
 /* Release tails can point anywhere as pressure fades. They must not undo
  * a left/right/back flick, even without an immediate perfect zero sample. */
 for(int dir=-1;dir<=1;dir+=2){
   struct NativeFlick f={0};uint64_t t=1000000;native_flick_sample(&f,t,0,0,1);
   for(int i=0;i<12;i++){t+=16667;native_flick_sample(&f,t,dir*156,0,1);}
   assert(fabsf(f.pending-dir*90)<.01f);
   for(int i=0;i<12;i++){t+=16667;native_flick_sample(&f,t,0,30,1);}
   assert(fabsf(f.pending-dir*90)<.01f);
 }
 struct NativeFlick tap={0};native_flick_sample(&tap,1000000,0,0,1);
 native_flick_sample(&tap,1010000,-80,1,1);native_flick_sample(&tap,1020000,0,0,1);
 native_flick_sample(&tap,1140000,0,0,1);assert(fabsf(tap.pending+90)<.01f);
 struct NativeFlick circle={0};uint64_t ct=1000000;native_flick_sample(&circle,ct,0,0,1);
 for(int i=0;i<12;i++){ct+=16667;native_flick_sample(&circle,ct,0,156,1);}
 for(int i=1;i<=72;i++){float a=i*6.28318530718f/72;ct+=16667;
   native_flick_sample(&circle,ct,(int)roundf(156*sinf(a)),(int)roundf(156*cosf(a)),1);}
 assert(fabsf(circle.pending-360)<1);
 puts("PASS Flick Stick: directional angles, 30/60Hz timing, edge rotation/wrap, consume-once, unchanged movement, default stick look and menu/suspend/stall resets");

 puts("PASS HID gyro integration: degrees/sec, consume-once, aim-only, menus and suspend discard motion");
 puts("PASS SELECT no longer changes weapon rendering; menu input and exit chord preserved");
 puts("PASS actual HID sampler: Vesper A/Jinx D-right taps survive intervening polls/releases, fire once, and clear on menu entry");
}
