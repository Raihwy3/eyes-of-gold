#include <initializer_list>
#include "native_weapon_stereo.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
struct Row {float x,y,z,w;};struct Matrix{Row r[4];};
static float dot(const Row&r,float x,float y,float z,float w){return r.x*x+r.y*y+r.z*z+r.w*w;}
int main(){
 Matrix flat={{{0,1,0,0},{-1,0,0,0},{0,0,1,0},{0,0,0,1}}},left,right;
 native_weapon_stereo(left,flat,0,false);assert(!memcmp(&left,&flat,sizeof flat));
 unsigned cases=0;
 for(int level=1;level<=10;level++)for(int step=0;step<=100;step++)for(float w:{1.f,10.f,1000.f}){
  float slider=level/10.f,z=-w*step/100.f;
  native_weapon_stereo(left,flat,slider,false);native_weapon_stereo(right,flat,slider,true);
  float centre=dot(flat.r[1],.2f,.3f,z,w)/w;
  float l=dot(left.r[1],.2f,.3f,z,w)/w-centre;
  float r=dot(right.r[1],.2f,.3f,z,w)/w-centre;
  float expected=-slider*(1.f+4.f*step/100.f)/200.f;
  assert(std::fabs(l-expected)<1e-6f && std::fabs(r+expected)<1e-6f);
  assert(!memcmp(&left.r[2],&flat.r[2],sizeof(Row)) && !memcmp(&left.r[3],&flat.r[3],sizeof(Row)));
  native_world_stereo(left,flat,slider,false);native_world_stereo(right,flat,slider,true);
  l=dot(left.r[1],.2f,.3f,z,w)/w-centre;
  r=dot(right.r[1],.2f,.3f,z,w)/w-centre;
  expected=-slider*(-2.f+12.f*step/100.f)/200.f;
  assert(std::fabs(l-expected)<1e-6f && std::fabs(r+expected)<1e-6f);
  assert(!memcmp(&left.r[2],&flat.r[2],sizeof(Row)) && !memcmp(&left.r[3],&flat.r[3],sizeof(Row)));
  cases++;
 }
 native_weapon_stereo(left,flat,2,false);native_weapon_stereo(right,flat,1,false);assert(!memcmp(&left,&right,sizeof left));
 native_world_stereo(left,flat,0,false);assert(!memcmp(&left,&flat,sizeof flat));
 native_world_stereo(left,flat,2,false);native_world_stereo(right,flat,1,false);assert(!memcmp(&left,&right,sizeof left));
 printf("PASS %u weapon and world projection cases: opposite eyes, depth variation, slider, unchanged Z/W and rotated LCD\n",cases);
}
