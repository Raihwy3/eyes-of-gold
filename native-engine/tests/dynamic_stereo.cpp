#include "native_dynamic_stereo.h"
#include "native_stereo_options.h"
#include "rt64_stereo_reference.h"
#include "native_weapon_stereo.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>
using namespace native_stereo;
struct Row{float x,y,z,w;};struct Matrix{Row r[4];};
int main(){
 unsigned cases=0;
 for(float n:{1.f,10.f,100.f})for(float far:{5000.f,30000.f})for(float scale:{.25f,1.f,2.f}){
  float a=(n+far)/(n-far)*scale,b=2*n*far/(n-far)*scale;
  auto p=Projection::from(a,-scale,b,0);assert(p.valid);
  for(float factor:{1.2f,2.f,10.f,30.f})for(float ds:{-1.f,-.75f}){
   float z=n*factor,w=z*scale,ndc=a/(-scale)+b/w;
   float device=ds*(ndc-1)*.5f+.025f;
   if(device<=0 || device>=1)continue;
   uint32_t packed=uint32_t(std::round(device*16777215.f))|0xab000000u;
   assert(std::fabs(p.distance(packed,ds,.025f)-w)<w*.001f);cases++;
  }
  assert(p.distance(0,-1,0)==0 && p.distance(0xffffff,-1,0)==0);
 }
 assert(!Projection::from(-1,0,-20,1).valid);
 assert(!Projection::from(NAN,-1,-20,0).valid);
 for(unsigned w:{240u,256u,480u,512u}){
  std::vector<bool> seen(w*512);
  for(unsigned y=0;y<512;y++)for(unsigned x=0;x<w;x++){
   unsigned at=tiled_index(x,y,w);assert(at<seen.size() && !seen[at]);seen[at]=true;
  }
 }
 unsigned reads=0;
 float near=near_depth([&](float x,float y){reads++;assert(x>=0 && x<=1 && y>=0 && y<=1);return x<.13f && y<.15f?.01f:.5f;});
 assert(reads==1500 && near==.5f);
 assert(near_depth([](float,float){return 0.f;})<0);
 assert(near_depth([](float,float){return NAN;})<0);
 Convergence a,b;
 assert(std::fabs(a.update(100,1,1.f/60)-100)<.001f); // initialise at target, no lunge from 600
 for(unsigned i=0;i<120;i++)a.update(100,1,1.f/60);
 for(unsigned i=0;i<60;i++)b.update(100,1,1.f/30);
 assert(a.value==100 && std::fabs(a.value-b.value)<.01f);
 float held=a.value;assert(a.update(0,1,.016f)==held && a.update(NAN,1,.016f)==held);
 a.reset();for(unsigned i=0;i<20;i++)a.update(300,1,1.f/60);
 a.update(10,1,1.f/60);a.update(8,1,1.f/60);assert(std::fabs(a.value-10.f)<.001f); // recent median, not last sample
 for(unsigned i=0;i<1000;i++)a.update(10000,1,1.f/60);
 assert(a.value>2999 && a.value<=3000);
 // Compare the application wrapper directly with the unmodified reference
 // control loop through noisy motion, missing depth and repeated close cuts.
 a.reset();eog_stereo_reference_test::AutoConvergence ref;eog_stereo_reference_test::AutoConvTuning tune;
 unsigned seed=13;
 for(unsigned i=0;i<2000;i++){
  seed=seed*1664525u+1013904223u;
  float z=i%37==0?0.f:i%110<50?float(100+seed%20):float(1000+seed%200);
  float expected=ref.update(z,manual_convergence,separation,0,tune);
  assert(std::fabs(a.update(z,1,1.f/60)-expected)<.005f);
 }
 AimDepth aim;
 unsigned aim_reads=0;
 auto surface=[&](float,float){aim_reads++;return 300.f;};
 assert(aim.sample(surface,200,120,400,1,600,1.f/60)==300);
 assert(aim_reads==13*25);
 float old_shift=aim.shift(600,1);
 assert(aim.sample([](float,float){return 0.f;},200,120,400,1,600,1.f/60)==300);
 assert(aim.shift(600,1)==old_shift); // sky never drops reticle depth
 assert(std::fabs(aim.shift(300,1))<1.e-6f); // follows world convergence, not a separate pixel EMA
 aim.reset();assert(aim.shift(600,1)==0 && !aim.have);
 assert(aim.sample([](float,float){return 0.f;},200,120,400,1,600,1.f/60)<0);
 // Median rejects a single foreground speck in each candidate patch.
 aim.reset();unsigned ix=0;
 assert(aim.sample([&](float,float){return ++ix%25==1?1.f:300.f;},200,120,400,1,600,1.f/60)==300);
 // At an edge, choose the nearest consistent candidate, not a three-step
 // iteration that can oscillate between foreground and background.
 aim.reset();float z=aim.sample([](float x,float){return x>=200?300.f:1200.f;},200,120,400,1,600,1.f/60);
 assert(z==300);
 for(float s:{0.f,.1f,.5f,1.f})for(float conv:{25.f,100.f,600.f})for(float z:{10.f,100.f,600.f,5000.f}){
  Matrix flat={{{0,1,0,0},{-1,0,0,0},{0,0,1,0},{0,0,0,1}}},l,r;
  native_world_stereo(l,flat,s,false);native_world_stereo(r,flat,s,true);
  float ls=l.r[1].w+native_stereo_eye_translation(s,false,false,conv)/z;
  float rs=r.r[1].w+native_stereo_eye_translation(s,true,false,conv)/z;
  assert(std::fabs(ls-disparity(z,conv,s))<1.e-6f && std::fabs(ls+rs)<1.e-6f);
  native_depth_stereo(l,flat,1.f,false,-ls*200.f);
  native_depth_stereo(r,flat,1.f,true,-ls*200.f);
  assert(std::fabs(l.r[1].w-ls)<1.e-6f && std::fabs(l.r[1].w+r.r[1].w)<1.e-6f);
 }
 for(float slider:{0.f,.25f,1.f})for(bool right:{false,true}){
  Matrix flat={{{0,1,0,0},{-1,0,0,0},{0,0,1,0},{0,0,0,1}}},sky;
  native_sky_stereo(sky,flat,slider,right);
  float lo=sky.r[1].x+sky.r[1].w,hi=-sky.r[1].x+sky.r[1].w;
  assert(lo<=-1.f+1.e-6f && hi>=1.f-1.e-6f);
  assert(std::fabs(std::fabs(sky.r[1].w)-separation*slider)<1.e-6f);
  assert(!memcmp(&sky.r[2],&flat.r[2],sizeof(Row)) && !memcmp(&sky.r[3],&flat.r[3],sizeof(Row)));
 }
 for(int manual_percent:{10,50,100,200})for(int comfort:{-20,0,30}){
  Convergence custom;eog_stereo_reference_test::AutoConvergence expected;
  eog_stereo_reference_test::AutoConvTuning tune;float manual=native_stereo_options::convergence(manual_percent);
  for(int frame=0;frame<200;frame++){
   float z=frame<100?200.f:700.f;
   float actual=custom.update(z,1,1.f/60,4,manual,comfort);
   assert(fabsf(actual-expected.update(z,manual,separation,comfort,tune))<.005f);
  }
 }
 assert(native_stereo_options::hud_pixels(50)==0 && native_stereo_options::hud_pixels(75)==4);
 assert(native_stereo_options::convergence(100)==3000);
 for(int c=50;c<=100;c+=5)for(int f=0;f<=20;f++)for(int colour=0;colour<=255;colour++){
  auto ghost=native_stereo_options::ghost(c,f);float pixel=colour/255.f;
  float expected=((pixel-.5f)*(c/100.f)+.5f)*(1-f/100.f)+f/100.f;
  assert(fabsf(pixel*ghost.a+ghost.b-expected)<.000001f);
 }
 puts("PASS configurable convergence/pop-out against RT64, unchanged default HUD plane and independent ghost affine reference");
 assert(disparity(0,600,1)==separation && disparity(600,600,1)==0);
 puts("PASS adaptive stereo: scaled perspective/depth-range inversion, D24S8, tiled native/AA/filter targets, full 1500-sample grid, reference-equivalent convergence, recent-median cuts, 13x5x5 aim strip, sky hold, edge/outlier rejection, matching reticle/world eye signs");
 printf("PASS %u projection/depth round trips\n",cases);
}
