#include "native_clip_colour.h"
#include "native_rsp_profile.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
static unsigned reference(float v){return unsigned(std::clamp(std::floor(v+.5f),0.f,255.f));}
int main() {
 unsigned samples=0;
 for(unsigned a=0;a<256;a++)for(unsigned b=0;b<256;b++)for(unsigned k=0;k<=32;k++) {
   float v=float(a)+(float(b)-a)*(float(k)/32);assert(native_clip_colour(v)==reference(v));samples++;
 }
 for(int k=-300;k<600;k++) {
   float half=k+.5f;
   for(float v: {half,std::nextafter(half,-INFINITY),std::nextafter(half,INFINITY)})assert(native_clip_colour(v)==reference(v));
 }
 using namespace native_rsp_profile;reset(true);
 {Scope outer(batch);{Scope child(triangle);{Scope pack(packing);note_backend(1);}}}
 assert(!active && elapsed[batch]>=elapsed[triangle] && elapsed[triangle]>=elapsed[packing]);
 assert(exclusive[batch]+exclusive[triangle]+exclusive[packing]+exclusive[backend]==elapsed[batch]);
 reset(false);{Scope disabled(triangle);note_backend(1);}assert(!calls[triangle] && !elapsed[backend] && !active);
 printf("PASS clipping colour conversion: %u byte-endpoint interpolations, half-integer neighbours and saturation match floorf; nested CPU timings count child work once\n",samples);
}
