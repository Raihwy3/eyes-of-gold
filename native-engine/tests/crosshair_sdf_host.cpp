#include "native_crosshair_sdf.h"
#include "conker_sdf_reference.h"
#include <cassert>
#include <cstdio>
int main(){
 uint32_t actual[4096],expected[4096];
 for(int style=1;style<=7;style++){
  native_crosshair_sdf_generate(style,actual);conker_reference(style,expected);
  unsigned filled=0,soft=0,black=0;
  for(int i=0;i<4096;i++){
   unsigned a=actual[i]&255,b=expected[i]&255;
   // C/C++ floating-point rounding can differ by one coverage level.
   assert(std::abs(int(a)-int(b))<=1);
   assert(std::abs(int(actual[i]>>24)-int(expected[i]>>24))<=1);
   filled+=a>0;soft+=a>0 && a<255;black+=a>0 && (actual[i]>>24)==0;
  }
  assert(filled>40 && filled<2300 && soft>20 && black>20);
  for(int i=0;i<64;i++)assert(actual[i]==0 && actual[63*64+i]==0 && actual[i*64]==0 && actual[i*64+63]==0);
 }
 puts("PASS all seven smooth crosshairs match Conker reference: fill, outline, partial coverage and transparent borders");
}
