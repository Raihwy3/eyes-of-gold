#include "native_motion_sensor.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomment"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "../vendor/GamepadMotionHelpers/GamepadMotion.hpp"
#pragma GCC diagnostic pop
#include <cmath>
static GamepadMotion motion;
static uint64_t previous;
static int filter_enabled,last_space=-1,last_axis=-1;
static float filtered_yaw,filtered_pitch,gravity[3];
static bool gravity_ready;
extern "C" void native_motion_set_filter(int enabled){__atomic_store_n(&filter_enabled,enabled!=0,__ATOMIC_RELAXED);}
static void clear_output_filter(){filtered_yaw=filtered_pitch=0;last_space=last_axis=-1;}
static int request,status,progress;
static uint64_t calibration_started;
static float still_time;
static unsigned samples;
static float mean_gyro[3],mean_accel[3];
static void restart_window(){still_time=0;samples=0;__atomic_store_n(&progress,0,__ATOMIC_RELAXED);}
extern "C" void native_motion_reset(void){motion.Reset();gravity_ready=false;clear_output_filter();motion.ResetContinuousCalibration();motion.PauseContinuousCalibration();previous=calibration_started=0;restart_window();__atomic_store_n(&request,0,__ATOMIC_RELAXED);__atomic_store_n(&status,0,__ATOMIC_RELAXED);}
extern "C" void native_motion_calibrate(void){
 // Repeated activation while already sampling must not restart the countdown.
 if(__atomic_load_n(&status,__ATOMIC_RELAXED)>0)return;
 __atomic_store_n(&progress,0,__ATOMIC_RELAXED);
 __atomic_store_n(&status,1,__ATOMIC_RELAXED);
 __atomic_store_n(&request,1,__ATOMIC_RELAXED);
}
extern "C" int native_motion_calibration_status(void){return __atomic_load_n(&status,__ATOMIC_RELAXED);}
extern "C" int native_motion_calibration_progress(void){return __atomic_load_n(&progress,__ATOMIC_RELAXED);}
extern "C" void native_motion_sample(uint64_t now,float gx,float gy,float gz,float ax,float ay,float az,int active,int space,int axis,float *yaw,float *pitch){
 *yaw=*pitch=0;
 if(__atomic_exchange_n(&request,0,__ATOMIC_RELAXED)){calibration_started=now;gravity_ready=false;clear_output_filter();restart_window();motion.PauseContinuousCalibration();__atomic_store_n(&status,1,__ATOMIC_RELAXED);}
 uint64_t last=previous;previous=now;
 bool calibrating=native_motion_calibration_status()>0;
 // Check elapsed wall time even when all samples are missing, invalid or late.
 // A failed attempt keeps the last good bias because we don't replace it until
 // a complete stable window has been collected.
 if(calibrating && now>=calibration_started && now-calibration_started>=12000000){
  restart_window();__atomic_store_n(&status,-2,__ATOMIC_RELAXED);return;
 }
 if(!last || now<=last || now-last>100000){
  motion.ResetMotion();gravity_ready=false;clear_output_filter();
  if(calibrating){restart_window();__atomic_store_n(&status,1,__ATOMIC_RELAXED);}
  return;
 }
 bool valid=std::isfinite(gx)&&std::isfinite(gy)&&std::isfinite(gz)&&std::isfinite(ax)&&std::isfinite(ay)&&std::isfinite(az);
 float dt=(now-last)*.000001f;
 if(calibrating){
  float g[3]={gx,gy,gz},a[3]={ax,ay,az};
  float a2=ax*ax+ay*ay+az*az;
  // Judge stillness from changes, not distance from zero angular velocity:
  // a constant sensor bias is precisely what manual calibration must learn.
  bool still=valid && a2>.01f && a2<64.f;
  if(still && samples){
   float da2=0,base2=0;
   for(int i=0;i<3;i++){
    still=still && std::abs(g[i]-mean_gyro[i])<2.f;
    da2+=(a[i]-mean_accel[i])*(a[i]-mean_accel[i]);base2+=mean_accel[i]*mean_accel[i];
   }
   still=still && da2<base2*.0064f; // Eight percent of the resting vector.
  }
  if(!still){restart_window();__atomic_store_n(&status,1,__ATOMIC_RELAXED);}
  else {
   samples++;still_time+=dt;
   for(int i=0;i<3;i++){
    if(samples==1){mean_gyro[i]=g[i];mean_accel[i]=a[i];}
    else {mean_gyro[i]+=(g[i]-mean_gyro[i])/samples;mean_accel[i]+=(a[i]-mean_accel[i])/samples;}
   }
   __atomic_store_n(&progress,std::min(100,(int)(still_time*100.f/3.f)),__ATOMIC_RELAXED);
   __atomic_store_n(&status,2,__ATOMIC_RELAXED);
  }
 }
 if(!valid){clear_output_filter();return;}
 // Never learn intentional slow aiming as drift. Menus/disengaged motion
 // permit the helper's hybrid stillness + sensor-fusion calibration.
 motion.SetCalibrationMode(active || calibrating?GamepadMotionHelpers::CalibrationMode::Manual:
  GamepadMotionHelpers::CalibrationMode::Stillness|GamepadMotionHelpers::CalibrationMode::SensorFusion);
 motion.ProcessMotion(gx,gy,gz,ax,ay,az,dt);
 if(calibrating){
  if(still_time>=3.f && samples>=30){
   motion.SetCalibrationOffset(mean_gyro[0],mean_gyro[1],mean_gyro[2],1);
   motion.SetAutoCalibrationConfidence(1.f);
   motion.ResetMotion();
   __atomic_store_n(&progress,100,__ATOMIC_RELAXED);
   __atomic_store_n(&status,-1,__ATOMIC_RELAXED);
  }
  return;
 }
 if(!active){clear_output_filter();}
 // Anchor the reference vertical to measured acceleration. Accumulating
 // gyro orientation is unnecessary for this projection and can couple yaw
 // into pitch when its estimate drifts. A short time-based low-pass rejects
 // sensor noise; constant body yaw does not rotate the vertical vector.
 float magnitude=std::sqrt(ax*ax+ay*ay+az*az);
 if(magnitude>.1f && magnitude<8.f){
  float a[3]={-ax/magnitude,-ay/magnitude,-az/magnitude};
  float blend=gravity_ready?1.f-std::exp(-dt/.025f):1.f;
  for(int i=0;i<3;i++)gravity[i]+=blend*(a[i]-gravity[i]);
  float length=std::sqrt(gravity[0]*gravity[0]+gravity[1]*gravity[1]+gravity[2]*gravity[2]);
  if(length>.0001f){for(float &v:gravity)v/=length;gravity_ready=true;}
 }
 if(!active)return;
 float p=0,y=0,z=0;
 motion.GetCalibratedGyro(p,y,z);
 if(space){
  if(!gravity_ready){clear_output_filter();return;}
  float xg=gravity[0],yg=gravity[1],zg=gravity[2];
  float length=std::sqrt(xg*xg+yg*yg+zg*zg);
  // Both library projections require a normalized reference vertical.
  if(gravity_ready && length>.0001f){
   xg/=length;yg/=length;zg/=length;
   if(space==1)GamepadMotion::CalculateWorldSpaceGyro(p,y,p,y,z,xg,yg,zg);
   else GamepadMotion::CalculatePlayerSpaceGyro(p,y,p,y,z,xg,yg,zg,1.f);
   // Convert the helper's gravity-relative turn sign to the 3DS camera
   // convention. Pitch and the exclusive local-axis path keep their signs.
   y=-y;
  }
 }else if(axis){
  // Tilting the left edge toward the player must turn left.
  y=-z;
 }
 // Degrees in, degrees out: both axes have the same gain. No axis blending
 // in local space. Bound sensor spikes without changing normal sensitivity.
 y=std::clamp(y,-600.f,600.f);p=std::clamp(p,-600.f,600.f);
 if(__atomic_load_n(&filter_enabled,__ATOMIC_RELAXED)){
  if(last_space!=space || last_axis!=axis)clear_output_filter();
  // The same cutoff applies to both axes. Filter small hand/sensor jitter,
  // then raise the cutoff with intentional movement to avoid a slow cursor.
  float speed=std::sqrt(y*y+p*p);
  float alpha=1.f-std::exp(-6.28318530718f*(4.f+speed)*dt);
  filtered_yaw+=alpha*(y-filtered_yaw);filtered_pitch+=alpha*(p-filtered_pitch);
  y=filtered_yaw;p=filtered_pitch;last_space=space;last_axis=axis;
 }else clear_output_filter();
 *yaw=y*dt;*pitch=p*dt;
}
