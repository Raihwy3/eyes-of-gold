/* Run the actual HID sampler with deterministic polling between game ticks. */
#include <3ds.h>
#include "native_input.h"
#include "native_pad.h"
#include "native_settings.h"
#include "native_flick.h"
#include "jsm_smoothing_reference.h"
#include <math.h>
#include <assert.h>
#include <stdio.h>
angularRate test_gyro_rate;
accelVector test_accel_vector={0,0,-512};
static unsigned held,render_state_extra;
static int gameplay=1,rx,ry;
void hidScanInput(void){}
u32 hidKeysHeld(void){return held;}
void hidCircleRead(circlePosition *p){p->dx=50;p->dy=-50;}
void hidCstickRead(circlePosition *p){p->dx=rx;p->dy=ry;}
int native_game_input_gameplay(void){return gameplay;}
unsigned native_game_render_state(void){return gameplay?(1u|render_state_extra):0u;}
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
 for(int hz=30;hz<=100;hz=hz==30?60:hz==60?100:101)for(int dir=-1;dir<=1;dir+=2) {
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
 native_flick_sample(&slow,1153334,0,0,1);native_flick_sample(&slow,1175001,0,0,1);native_flick_sample(&slow,1191668,0,-60,1);
 native_flick_sample(&slow,1311668,0,-60,1);assert(fabsf(slow.pending-90)<.02f);
 struct NativeFlick wrap={0};native_flick_sample(&wrap,1000000,0,0,1);
 native_flick_sample(&wrap,1010000,1,-156,1);native_flick_sample(&wrap,1050000,1,-156,1);
 wrap.pending=wrap.remaining=0;wrap.remaining_us=0;
 native_flick_sample(&wrap,1060000,-1,-156,1);assert(fabsf(wrap.pending)<1); // Crossing the back axis must not jump by 360 degrees.
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
 native_flick_sample(&tap,1140000,0,0,1);assert(fabsf(tap.pending+90)<1.f);
 struct NativeFlick circle={0};uint64_t ct=1000000;native_flick_sample(&circle,ct,0,0,1);
 for(int i=0;i<12;i++){ct+=16667;native_flick_sample(&circle,ct,0,156,1);}
 for(int i=1;i<=72;i++){float a=i*6.28318530718f/72;ct+=16667;
   native_flick_sample(&circle,ct,(int)roundf(156*sinf(a)),(int)roundf(156*cosf(a)),1);}
 for(int i=0;i<8;i++){ct+=16667;native_flick_sample(&circle,ct,0,156,1);}
 assert(fabsf(circle.pending-360)<1);
 /* Upstream absolute-time quadratic easing: 43.75% at 25ms,
  * 75% at 50ms, and 100% at 100ms. Repeated timestamps consume nothing. */
 struct NativeFlick eased={0};native_flick_sample(&eased,1000000,0,0,1);
 native_flick_sample(&eased,1010000,80,0,1);
 native_flick_sample(&eased,1035000,80,0,1);assert(fabsf(eased.pending-39.375f)<.001f);
 native_flick_sample(&eased,1035000,80,0,1);assert(fabsf(eased.pending-39.375f)<.001f);
 native_flick_sample(&eased,1060000,80,0,1);assert(fabsf(eased.pending-67.5f)<.001f);
 native_flick_sample(&eased,1110000,80,0,1);assert(fabsf(eased.pending-90)<.001f);
 /* Compare the adapted smoother with the upstream function, including
  * wraparound of its history and the immediate/smoothed blend boundary. */
 for(int count=1;count<=64;count=count==1?3:count==3?6:count==6?64:65){
   struct NativeFlick actual={0},reference={0};actual.sample_count=count;
   for(int n=0;n<2000;n++){
     float value=n%11==0?0:sinf(n*.73f)*15;
     float a=native_flick_smooth(&actual,value);
     float b=jsm_reference_smooth(&reference,value,.08f*57.2957795131f,.16f*57.2957795131f,count);
     assert(fabsf(a-b)<.00001f);
   }
 }
 /* Low-pressure rotation in sub-three-degree steps must retain every step. */
 struct NativeFlick fine={0};uint64_t ft=1000000;native_flick_sample(&fine,ft,0,0,1);
 for(int i=0;i<12;i++){ft+=16667;native_flick_sample(&fine,ft,0,55,1);}
 for(int i=1;i<=45;i++){float a=i*0.01745329252f;ft+=16667;
   native_flick_sample(&fine,ft,(int)roundf(55*sinf(a)),(int)roundf(55*cosf(a)),1);}
 for(int i=0;i<8;i++){ft+=16667;native_flick_sample(&fine,ft,39,39,1);}
 assert(fabsf(fine.pending-45)<1);
 float before=fine.pending;
 ft+=16667;native_flick_sample(&fine,ft,0,0,1);
 ft+=16667;native_flick_sample(&fine,ft,39,39,1);
 assert(fabsf(fine.pending-before)<.01f); // A single dropout does not re-flick.
 native_flick_sample(&fine,ft-1,0,55,1);assert(!fine.active && fine.pending==0);
 /* C-stick pressure near a cardinal should produce an exact initial turn,
  * while intentional diagonals retain their angle. */
 for(int direction=-1;direction<=1;direction+=2){
  struct NativeFlick f={0};native_flick_sample(&f,1000000,0,0,1);
  native_flick_sample(&f,1010000,direction*156,20,1);
  native_flick_sample(&f,1120000,direction*156,20,1);
  assert(fabsf(f.pending-direction*90)<.001f);
 }
 struct NativeFlick diagonal={0};native_flick_sample(&diagonal,1000000,0,0,1);
 native_flick_sample(&diagonal,1010000,100,100,1);native_flick_sample(&diagonal,1120000,100,100,1);
 assert(fabsf(diagonal.pending-45)<.001f);
 /* Flick mode must not eat the C-stick's ordinary free-aim input. */
 native_settings_init(NULL);native_setting_set_style(8);native_setting_set_flick(1);native_input_init();
 gameplay=1;held=KEY_L;rx=156;ry=80;inputUpdate();
 signed char aim_x=0,aim_y=0;inputComputePad(0,&aim_x,&aim_y);
 assert(aim_x==80 && aim_y>35);
 float aim_flick=1;native_input_take_flick(&aim_flick);assert(aim_flick==0);
 held=0;rx=ry=0;inputUpdate();
 /* Snapping a diagonal flick near a cardinal must absorb settling into that
  * cardinal rather than adding another ~10 degrees on follow-up samples. */
 struct NativeFlick settle={0};native_flick_sample(&settle,1000000,0,0,1);
 native_flick_sample(&settle,1010000,48,8,1);
 native_flick_sample(&settle,1120000,156,0,1);
 assert(fabsf(settle.pending-90)<.01f);
 /* Stiff New 3DS C-sticks peaking/sweeping around r=36..42 must start and
  * keep tracking without dropping mid-sweep. */
 struct NativeFlick nub={0};uint64_t nt=1000000;native_flick_sample(&nub,nt,0,0,1);
 native_flick_sample(&nub,nt+=16667,0,42,1);
 for(int i=0;i<8;i++)native_flick_sample(&nub,nt+=16667,0,42,1);
 native_flick_sample(&nub,nt+=16667,36,0,1);
 for(int i=0;i<8;i++)native_flick_sample(&nub,nt+=16667,36,0,1);
 assert(fabsf(nub.pending-90)<1.f);
 /* Tank mode (render_state bit 8) must keep C-stick turret control in Vesper Flick mode. */
 render_state_extra=8u;native_input_init();gameplay=1;held=0;rx=156;ry=-80;inputUpdate();
 inputComputePad(0,&aim_x,&aim_y);
 assert(aim_x==80 && aim_y<-35);
 aim_flick=1;native_input_take_flick(&aim_flick);assert(aim_flick==0);
 render_state_extra=0;rx=ry=0;
 /* Full HID gyro integration across Local+Roll, World and Player modes at
  * flat, 45-degree and upright console poses. */
 native_setting_set_gyro(2);native_setting_set_gyro_filter(0);
 native_setting_set_gyro_space(0);native_setting_set_gyro_axis(1);native_input_init();
 test_accel_vector=(accelVector){0,0,-512};
 test_gyro_rate=(angularRate){0,0,-1438};
 total_y=total_p=0;
 for(int i=0;i<60;i++){inputUpdate();native_input_take_gyro(&gy,&gp);total_y+=gy;total_p+=gp;}
 assert(total_y>95 && total_y<105 && fabsf(total_p)<.01f);
 for(int space=1;space<=2;space++){
  native_setting_set_gyro_space(space);native_setting_set_gyro_axis(0);
  const accelVector poses[3]={{0,0,-512},{0,362,-362},{0,512,0}};
  const angularRate rates[3]={{0,1438,0},{0,1017,1017},{0,0,1438}};
  for(int p=0;p<3;p++){
   native_input_init();test_accel_vector=poses[p];test_gyro_rate=(angularRate){0,0,0};
   for(int i=0;i<20;i++){inputUpdate();native_input_take_gyro(&gy,&gp);}
   test_gyro_rate=rates[p];total_y=total_p=0;
   for(int i=0;i<60;i++){inputUpdate();native_input_take_gyro(&gy,&gp);total_y+=gy;total_p+=gp;}
   assert(total_y>95 && total_y<105 && fabsf(total_p)<.25f);
  }
 }
 test_accel_vector=(accelVector){0,0,-512};test_gyro_rate=(angularRate){0,0,0};
 native_setting_set_gyro(0);native_setting_set_gyro_space(0);native_setting_set_gyro_axis(0);
 /* 1.7 Custom full rebinding: Southpaw sticks, custom buttons, digital aim, and safe menus. */
 struct NativeCustomBinds cb={
  EOG_STICKS_SOUTHPAW,EOG_BIND_ZR,EOG_BIND_ZL,EOG_BIND_Y,
  EOG_BIND_X,EOG_BIND_SELECT,EOG_BIND_DDOWN,EOG_DIGITAL_DPAD,EOG_DIGITAL_OFF
 };
 struct NativePad cp=native_pad_map_custom(EOG_ZR|EOG_ZL|EOG_Y|EOG_X|EOG_SELECT|EOG_DRIGHT,156,-78,40,80,10,1,&cb);
 assert((cp.buttons&0x2000) && (cp.buttons&0x10) && (cp.buttons&0x4000) && (cp.buttons&0x8000));
 assert((cp.actions&EOG_INTERACT) && (cp.actions&EOG_RELOAD) && !(cp.actions&EOG_CROUCH));
 assert(cp.x==80 && cp.y< -30 && cp.move_x>15 && cp.move_y>35);
 /* Rebound A/B must never break menu accept/back. */
 cp=native_pad_map_custom(EOG_A|EOG_B,0,0,0,0,10,0,&cb);
 assert((cp.buttons&0x8000) && (cp.buttons&0x4000) && !cp.actions);
 /* 1.8 Natalya: Circle Pad looks, X/B/Y/A move/strafe, D-pad actions; Swap aim stick flips them. */
 struct NativePad np=native_pad_map_custom(EOG_X|EOG_Y|EOG_DRIGHT,156,-78,0,0,11,1,NULL);
 assert(np.x==80 && np.y<-30 && np.move_x==-80 && np.move_y==80 && (np.actions&EOG_INTERACT));
 native_pad_apply_swap(&np,11,1,1);
 assert(np.move_x==80 && np.move_y<-30 && np.x==-80 && np.y==80);
 /* In live inputUpdate, Swap aim stick only swaps while aiming (L or render_state bit 4). */
 native_setting_set_flick(0);native_setting_set_style(8);native_setting_set_swap_sticks(1);native_input_init();
 gameplay=1;held=0;rx=0;ry=80;inputUpdate();
 inputComputePad(0,&aim_x,&aim_y);
 assert(aim_x==0 && aim_y>35); /* Not aiming: C-stick still aims */
 held=KEY_L;inputUpdate();
 inputComputePad(0,&aim_x,&aim_y);
 assert(aim_x>15 && aim_y< -15); /* Aiming: Circle Pad (50,-50) aims */
 native_setting_set_swap_sticks(0);held=0;rx=ry=0;
 puts("PASS flick-stick aiming: held aim restores C-stick X/Y and discards queued flick turns");
 puts("PASS Flick Stick: directional angles, 30/60Hz timing, edge rotation/wrap, consume-once, unchanged movement, default stick look and menu/suspend/stall resets");

 puts("PASS HID gyro integration: degrees/sec, consume-once, aim-only, menus and suspend discard motion");
 puts("PASS SELECT no longer changes weapon rendering; menu input and exit chord preserved");
 puts("PASS actual HID sampler: Vesper A/Jinx D-right taps survive intervening polls/releases, fire once, and clear on menu entry");
}
