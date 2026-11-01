#include "pica_inputs.h"
#include "native_unorm.h"
#include "pica_texcoord.h"
#include <cstring>
#include <cmath>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>
extern bool pica_plan_inputs_reference(const float *,size_t,size_t,size_t,unsigned,bool,PicaInputPlan *);
static bool same(PicaInputPlan a,PicaInputPlan b) {
 return a.primary==b.primary && a.constant==b.constant && a.scalar==b.scalar && a.scalar_alpha==b.scalar_alpha &&
 a.primary_alpha==b.primary_alpha && a.primary_fixed_alpha==b.primary_fixed_alpha;
}
int main() {
 for(unsigned i=0;i<256;i++) {
   volatile float numerator=float(i);float reference=numerator/255.f,value=native_unorm8(i);
   assert(!std::memcmp(&reference,&value,sizeof value));
 }
 puts("PASS all 256 byte colour conversions exactly match division");
 std::mt19937 rng(0x007);
 for(unsigned i=0;i<50000;i++) {
   unsigned gpu=8u<<(rng()%8),image=1+rng()%gpu;
   volatile float coordinate=(int(rng()%200000)-100000)/32.f;
   float original=coordinate*image/gpu,scaled=pica_texture_scaled(coordinate,image,1.f/gpu);
   assert(!std::memcmp(&original,&scaled,sizeof scaled));
 }
 puts("PASS 50000 padded texture coordinates exactly match the previous division path");
 for(unsigned trial=0;trial<30000;trial++) {
   unsigned inputs=2+trial%2,channels=3+(trial/2)%2,count=3*(1+rng()%64),stride=4+inputs*channels;
   std::vector<float> data(stride*count);
   for(unsigned input=0;input<inputs;input++) {
     unsigned kind=rng()%6,alpha_kind=rng()%4;float fixed=float(rng()%256)/255.f;
     for(unsigned v=0;v<count;v++) {
       float *p=data.data()+v*stride+4+input*channels;
       float scalar=kind%2?fixed:float(rng()%256)/255.f;
       for(unsigned c=0;c<3;c++)p[c]=kind<4?scalar:float(rng()%256)/255.f;
       if(channels==4)p[3]=alpha_kind==0?0:alpha_kind==1?1:alpha_kind==2?scalar:float(rng()%256)/255.f;
     }
   }
   PicaInputPlan a,b;
   bool ra=pica_plan_inputs_reference(data.data(),stride,4,count,inputs,channels==4,&a);
   bool rb=pica_plan_inputs(data.data(),stride,4,count,inputs,channels==4,&b);
   if(ra || !rb) assert(ra==rb && same(a,b));
   else {
     assert(inputs==2 && channels==4 && b.primary==1-b.scalar);
     bool scalar_constant=b.scalar_alpha==3 && b.constant==b.scalar;
     bool primary_constant=b.scalar_alpha==2 && b.constant==b.primary && b.primary_alpha==b.scalar && b.primary_fixed_alpha==2;
     assert(scalar_constant || primary_constant);
     unsigned alpha_slot=scalar_constant?b.scalar:b.primary;
     float first=data[4+alpha_slot*channels+3];
     for(unsigned v=0;v<count;v++) {
       const float *p=data.data()+v*stride+4+b.scalar*channels;
       float alpha_value=data[v*stride+4+alpha_slot*channels+3];
       assert(std::fabs(p[0]-p[1])<.00001f && std::fabs(p[1]-p[2])<.00001f && std::fabs(alpha_value-first)<.00001f);
     }
   }
 }
 puts("PASS material planner: 30000 mixed uniform/scalar/alpha batches exactly match the prior planner");
 /* Representative varying shade + uniform environment, the common 2-input
  * material. Timing is a host microbenchmark, not a console FPS prediction. */
 const unsigned count=192,stride=12;std::vector<float> data(count*stride,1.f);
 for(unsigned v=0;v<count;v++)for(unsigned c=0;c<3;c++)data[v*stride+4+c]=float((v+c*37)%256)/255.f;
 volatile int checksum=0;
 auto bench=[&](auto fn){auto start=std::chrono::steady_clock::now();
   for(unsigned i=0;i<30000;i++){PicaInputPlan p;bool ok=fn(data.data(),stride,4,count,2,true,&p);checksum=checksum+ok+p.primary;}
   return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};
 double before=bench(pica_plan_inputs_reference),after=bench(pica_plan_inputs);
 printf("Host material planning benchmark: prior %.2fms, current %.2fms (checksum %d); hardware timing pending\n",before,after,int(checksum));
 for(unsigned v=0;v<count;v++)for(unsigned c=0;c<3;c++)data[v*stride+4+c]=.5f;
 before=bench(pica_plan_inputs_reference);after=bench(pica_plan_inputs);
 printf("Host flat/UI material planning: prior %.2fms, current %.2fms (checksum %d)\n",before,after,int(checksum));
}
