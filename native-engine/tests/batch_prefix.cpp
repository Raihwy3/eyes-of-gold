#include "pica_batch.h"
#include <vector>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdint>
static size_t reference(const float*p,size_t s,size_t o,size_t n,unsigned inputs,bool alpha,bool fog,const float*const*refs){
 if(!n || !pica_batch_fits(p,s,o,1,inputs,alpha,fog,nullptr,nullptr,refs))return 0;
 size_t low=1,high=n+1;
 while(low+1<high){size_t mid=low+(high-low)/2;if(pica_batch_fits(p,s,o,mid,inputs,alpha,fog,nullptr,nullptr,refs))low=mid;else high=mid;}
 return low;
}
int main(){
 uint32_t rng=17;unsigned cases=0;double old_ms=0,new_ms=0;volatile size_t checksum=0;
 for(unsigned trial=0;trial<2000;trial++){
  rng=rng*1664525u+1013904223u;size_t n=1+(rng%256),run=1+((rng>>8)%n);
  bool alpha=trial&1,fog=trial&2;unsigned inputs=2+(trial%3==0),channels=alpha?4:3;
  size_t offset=4+(fog?4:0),stride=offset+inputs*channels;
  std::vector<float> data(n*3*stride),shuffled(data.size());std::vector<const float*> refs(n*3);
  for(size_t v=0;v<n*3;v++){
   float *p=data.data()+v*stride;size_t group=(v/3)/run;
   if(fog){p[4]=.2f+float((trial%4==0)?group%7:0)*.01f;p[5]=.3f;p[6]=.4f;p[7]=float(v%10)*.05f;}
   for(unsigned in=0;in<inputs;in++){
    float *c=p+offset+in*channels;
    if(in==0){c[0]=.1f+float(v%11)*.01f;c[1]=c[0]+.1f;c[2]=c[0]+.2f;}
    else if(in==1){c[0]=.4f+float(group%7)*.03f;c[1]=.23f;c[2]=.67f;}
    else c[0]=c[1]=c[2]=float(v%13)*.03f;
    if(alpha)c[3]=in==2?c[0]:.8f;
   }
  }
  for(size_t v=0;v<n*3;v++){size_t at=n*3-1-v;for(size_t j=0;j<stride;j++)shuffled[at*stride+j]=data[v*stride+j];refs[v]=shuffled.data()+at*stride;}
  for(unsigned indirect=0;indirect<2;indirect++){
   const float*const*view=indirect?refs.data():nullptr;
   auto a=reference(data.data(),stride,offset,n,inputs,alpha,fog,view);
   auto b=pica_batch_prefix(data.data(),stride,offset,n,inputs,alpha,fog,view);
   assert(a==b);cases++;
   if(run<=4){
    auto start=std::chrono::steady_clock::now();for(int rep=0;rep<20;rep++)checksum=checksum+reference(data.data(),stride,offset,n,inputs,alpha,fog,view);
    auto mid=std::chrono::steady_clock::now();for(int rep=0;rep<20;rep++)checksum=checksum+pica_batch_prefix(data.data(),stride,offset,n,inputs,alpha,fog,view);
    auto end=std::chrono::steady_clock::now();old_ms+=std::chrono::duration<double,std::milli>(mid-start).count();new_ms+=std::chrono::duration<double,std::milli>(end-mid).count();
   }
  }
 }
 printf("PASS %u material/fog prefixes match reference, including indirect vertices; short-run host benchmark old=%.3fms new=%.3fms checksum=%zu\n",cases,old_ms,new_ms,size_t(checksum));
}
