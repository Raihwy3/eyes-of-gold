#include "native_gyro.h"
#include <cassert>
#include <cmath>
#include <cstdio>
static float turn(unsigned step){
 NativeGyro g={};g.time=1;float total=0;
 for(unsigned i=0;i<1200000/step;i++){
  native_gyro_sample(&g,g.time+step,90,0,1);total+=g.yaw;g.yaw=g.pitch=0;
 }
 return total;
}
int main(){
 float a=turn(16666),b=turn(33333);assert(std::fabs(a-b)<.1f);
 NativeGyro g={};g.time=1;native_gyro_sample(&g,16667,90,90,1);assert(g.yaw>0 && g.pitch>0);
 native_gyro_sample(&g,200000,90,90,1);assert(g.yaw==0 && g.pitch==0);
 native_gyro_sample(&g,216667,NAN,INFINITY,1);assert(g.yaw==0 && g.pitch==0);
 native_gyro_sample(&g,233333,100000,100000,1);assert(g.yaw<=15 && g.pitch<=15);
 native_gyro_sample(&g,250000,90,90,0);assert(g.yaw==0 && g.pitch==0);
 NativeGyro quiet={};quiet.time=1;
 for(int i=0;i<120;i++)native_gyro_sample(&quiet,quiet.time+16666,.5f,.25f,0);
 assert(quiet.bias_yaw>.4f && quiet.yaw==0);
 float bias=quiet.bias_yaw;
 for(int i=0;i<60;i++){native_gyro_sample(&quiet,quiet.time+16666,.5f,.25f,1);quiet.yaw=quiet.pitch=0;}
 assert(quiet.bias_yaw==bias);
 puts("PASS gyro filter: 30/60Hz integration, long stalls, nonfinite input, spike bounds and inactive reset");
}
