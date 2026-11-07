#include "native_motion_sensor.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
static uint64_t now;
static void reset(){native_motion_reset();native_motion_set_filter(0);now=1000000;float y,p;native_motion_sample(now,0,0,0,0,1,0,0,0,0,&y,&p);}
static void sample(float gx,float gy,float gz,int active,int space,int axis,float &y,float &p){now+=10000;native_motion_sample(now,gx,gy,gz,0,1,0,active,space,axis,&y,&p);}
int main(){
 float y,p;
 for(int hz: {30,60,100}){
  reset();float yaw=0,pitch=0;
  for(int i=0;i<hz;i++){now+=1000000/hz;native_motion_sample(now,30,30,0,0,1,0,1,0,0,&y,&p);yaw+=y;pitch+=p;}
  assert(fabs(yaw-30)<.01 && fabs(pitch-30)<.01);
 }
 reset();sample(10,20,100,1,0,0,y,p);assert(fabs(y-.2f)<.00001 && fabs(p-.1f)<.00001);
 sample(10,100,-20,1,0,1,y,p);assert(fabs(y-.2f)<.00001 && fabs(p-.1f)<.00001);
 // Local roll uses the helper's Z axis. Its sign must match the
 // camera's horizontal convention, independently of pitch and local yaw.
 for(int direction: {-1,1}){
  reset();sample(10,100,direction*20.f,1,0,1,y,p);
  assert(y*direction<0 && fabs(y+direction*.2f)<.00001f);
  assert(fabs(p-.1f)<.00001f);
 }
 reset();for(int i=0;i<100;i++)sample(0,0,0,0,0,0,y,p);
 for(int space=1;space<=2;space++){sample(10,20,0,1,space,0,y,p);assert(std::isfinite(y)&&std::isfinite(p)&&fabs(y)>.1f && fabs(p)>.05f);}
 // The 3DS camera uses the opposite horizontal sign to the helper's
 // world/player convention. Check both directions in the normal playing
 // pose, with signed accelerometer input, rather than only output magnitude.
 for(int space=1;space<=2;space++)for(int direction: {-1,1}){
  reset();
  for(int i=0;i<200;i++){now+=10000;native_motion_sample(now,0,0,0,0,-1,0,1,space,0,&y,&p);}
  now+=10000;native_motion_sample(now,10,direction*20.f,0,0,-1,0,1,space,0,&y,&p);
  assert(y*direction>0 && fabs(y-direction*.2f)<.005f);
  assert(p>0 && fabs(p-.1f)<.005f);
 }
 // Real body yaw: angular velocity is parallel to vertical at every hold
 // angle. Left/right turns must cancel without producing downward pitch.
 for(int space=1;space<=2;space++)for(int hz: {30,60,100})for(float tilt: {0.f,.6f,1.2f}){
  reset();float ay=cosf(tilt),az=sinf(tilt),total_y=0,total_p=0;
  for(int i=0;i<20;i++){now+=1000000/hz;native_motion_sample(now,0,0,0,0,ay,az,1,space,0,&y,&p);}
  for(int i=0;i<hz*4;i++){
   float rate=(i<hz*2?1.f:-1.f)*45.f;
   now+=1000000/hz;native_motion_sample(now,0,rate*ay,rate*az,0,ay,az,1,space,0,&y,&p);
   assert(fabsf(p)<.001f);total_y+=y;total_p+=p;
  }
  assert(fabsf(total_y)<.05f && fabsf(total_p)<.01f);
 }
 // At a rolled holding angle World pitch rejects vertical-axis rotation;
 // Player pitch deliberately retains local X, as the reference specifies.
 float out_pitch[2];
 for(int space=1;space<=2;space++){
  reset();for(int i=0;i<30;i++){now+=10000;native_motion_sample(now,0,0,0,.6f,.8f,0,1,space,0,&y,&p);}
  now+=10000;native_motion_sample(now,18,24,0,.6f,.8f,0,1,space,0,&y,&p);out_pitch[space-1]=p;
 }
 assert(fabsf(out_pitch[0])<.001f && fabsf(out_pitch[1]-.18f)<.001f);
 // Optional jitter filter attenuates alternating noise, retains intentional
 // steady turns, and discards its tail when aiming is disengaged.
 reset();native_motion_set_filter(1);float noise=0;
 for(int i=0;i<100;i++){sample(0,i%2?1.f:-1.f,0,1,0,0,y,p);noise+=fabsf(y);}
 assert(noise<.25f);
 float travel=0;for(int i=0;i<100;i++){sample(30,30,0,1,0,0,y,p);travel+=y;assert(fabsf(y-p)<.02f);}
 assert(travel>29.f && travel<=30.f);
 sample(0,0,0,0,0,0,y,p);sample(0,0,0,1,0,0,y,p);assert(y==0 && p==0);
 native_motion_set_filter(0);
 puts("PASS moving World/Player reference: multi-angle body yaw reversals at 30/60/100Hz, distinct pitch axes, optional jitter attenuation and disengage reset");
 sample(30,30,0,0,0,0,y,p);assert(y==0 && p==0);
 now+=1000000;native_motion_sample(now,30,30,0,0,1,0,1,0,0,&y,&p);assert(y==0 && p==0);
 reset();native_motion_calibrate();
 for(int i=0;i<100;i++){sample(.5f,-.3f,.2f,0,0,0,y,p);assert(y==0 && p==0);}
 sample(20,0,0,0,0,0,y,p);assert(native_motion_calibration_status()==1);
 for(int i=0;i<310;i++)sample(.5f,-.3f,.2f,0,0,0,y,p);
 assert(native_motion_calibration_status()==-1);
 sample(.5f,-.3f,.2f,1,0,0,y,p);assert(fabs(y)<.00001 && fabs(p)<.00001);
 sample(10.5f,9.7f,.2f,1,0,0,y,p);assert(fabs(y-.1f)<.00001 && fabs(p-.1f)<.00001);
 // Recalibration after prior calibration and active World/Player use must
 // complete repeatedly, without blocking sensor polling or leaking motion.
 for(int cycle=0;cycle<40;cycle++){
  for(int i=0;i<30;i++)sample(10,-15,5,1,1+cycle%2,0,y,p);
  native_motion_calibrate();
  for(int i=0;i<310;i++){sample(.5f,-.3f,.2f,0,0,0,y,p);assert(y==0 && p==0);}
  assert(native_motion_calibration_status()==-1);
  sample(.5f,-.3f,.2f,1,0,0,y,p);assert(fabs(y)<.00001f && fabs(p)<.00001f);
 }
 // A steady raw offset above 3 dps and a resting acceleration vector
 // away from 1 g must not leave calibration waiting indefinitely.
 reset();native_motion_calibrate();
 for(int i=0;i<320 && native_motion_calibration_status()>0;i++){
  native_motion_calibrate(); // A held/repeated menu activation is ignored.
  now+=10000;float jitter=i%2?.15f:-.15f;
  native_motion_sample(now,6+jitter,-8-jitter,4,0,-2,0,0,0,0,&y,&p);
  assert(y==0 && p==0);
  if(i==100)assert(native_motion_calibration_progress()>=30);
 }
 assert(native_motion_calibration_status()==-1 && native_motion_calibration_progress()==100);
 now+=10000;native_motion_sample(now,6,-8,4,0,-2,0,1,0,0,&y,&p);
 assert(fabs(y)<.0001f && fabs(p)<.0001f);
 // Missing readings and continuous movement time out; neither replaces the
 // last successful bias. NaNs and long poll gaps also cannot bypass timeout.
 for(int failure=0;failure<3;failure++){
  native_motion_calibrate();
  for(int i=0;i<1300 && native_motion_calibration_status()>0;i++){
   now+=failure==2?200000:10000;
   if(failure==0)native_motion_sample(now,0,0,0,0,0,0,0,0,0,&y,&p);
   else if(failure==1)native_motion_sample(now,i%2?40:-40,0,0,0,-2,0,0,0,0,&y,&p);
   else native_motion_sample(now,NAN,0,0,0,-2,0,0,0,0,&y,&p);
   assert(y==0 && p==0);
  }
  assert(native_motion_calibration_status()==-2 && native_motion_calibration_progress()==0);
  now+=10000;native_motion_sample(now,6,-8,4,0,-2,0,1,0,0,&y,&p);
  assert(fabs(y)<.0001f && fabs(p)<.0001f);
 }
 puts("PASS calibration: biased/scaled noisy stationary sensors, progress, repeat activation, missing/moving/nonfinite/stalled timeouts and previous bias preserved");
 puts("PASS 40 repeated recalibrations after active World/Player input; menu-style polling continues and calibrated zero remains finite");
 puts("PASS motion backend: equal degree gain at 30/60/100Hz, exclusive local axes, signed Local Roll/World/Player turning, gravity spaces, inactive/stall discard, stillness calibration and drift subtraction");
}
