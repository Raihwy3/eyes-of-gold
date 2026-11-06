#include "native_weapon_stereo.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
struct Row{float x,y,z,w;};struct Matrix{Row r[4];};
static float dot(const Row&r,float x,float y,float z,float w){return r.x*x+r.y*y+r.z*z+r.w*w;}
int main(){
 Matrix flat={{{0,1,0,0},{-1,0,0,0},{0,0,1,0},{0,0,0,1}}},left,right;
 unsigned cases=0;
 for(int weapon=0;weapon<2;weapon++)for(int level=0;level<=10;level++)for(int d=10;d<=10000;d+=10){
  float slider=level/10.f,w=float(d);
  if(weapon){native_weapon_stereo(left,flat,slider,false);native_weapon_stereo(right,flat,slider,true);}
  else{native_world_stereo(left,flat,slider,false);native_world_stereo(right,flat,slider,true);}
  float previous=0;
  for(int depth=0;depth<3;depth++){
   float z=-w*depth/2.f,centre=dot(flat.r[1],.2f,.3f,z,w);
   float l=dot(left.r[1],.2f,.3f,z,w)-centre+native_stereo_eye_translation(slider,false,weapon);
   float r=dot(right.r[1],.2f,.3f,z,w)-centre+native_stereo_eye_translation(slider,true,weapon);
   l=(weapon?native_stereo_bounded_shift(l,w,slider):l)/w;
   r=(weapon?native_stereo_bounded_shift(r,w,slider):r)/w;
   assert(std::fabs(l+r)<1e-6f);
   if(weapon)assert(std::fabs(l)<=.03f*slider+1e-6f);
   // Changing near/far depth representation cannot change eye separation.
   if(depth)assert(std::fabs(l-previous)<1e-6f);
   previous=l;
   float pixels=slider*((weapon?160.f:1200.f)/w+(weapon?1.f:-2.f));
   float expected=-pixels/200.f,limit=.03f*slider;
   if(weapon && expected>limit)expected=limit;
   if(weapon && expected< -limit)expected=-limit;
   assert(std::fabs(l-expected)<1e-6f);
   assert(!memcmp(&left.r[2],&flat.r[2],sizeof(Row)) && !memcmp(&left.r[3],&flat.r[3],sizeof(Row)));
   cases++;
  }
 }
 native_world_stereo(left,flat,0,false);assert(!memcmp(&left,&flat,sizeof flat));
 native_world_stereo(left,flat,1,false);
 // World convergence at 600: zero disparity. A 300-unit surface is in front;
 // a 1200-unit surface is behind, rather than shifted onto one flat layer.
 assert(std::fabs((left.r[1].w*600+native_stereo_eye_translation(1,false,false))/600)<1e-6f);
 assert(left.r[1].w*300+native_stereo_eye_translation(1,false,false)<0);
 assert(left.r[1].w*1200+native_stereo_eye_translation(1,false,false)>0);
 // Reproduce the old flat near-world plane: these three walls all saturated
 // at the same six pixels. Off-axis world projection must distinguish them.
 float last=0;
 for(float distance=30.f;distance<=90.f;distance+=30.f){
  float shift=left.r[1].w+native_stereo_eye_translation(1,false,false)/distance;
  assert(std::fabs(native_stereo_bounded_shift(shift*distance,distance,1.f)/distance+.03f)<1e-6f);
  if(last)assert(std::fabs(shift-last)>.01f);
  last=shift;
 }
 printf("PASS %u stereo cases: inverse camera distance, convergence, both eyes, slider, depth-range independence and unchanged Z/W\n",cases);
}
