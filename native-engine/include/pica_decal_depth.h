#ifndef EOG_PICA_DECAL_DEPTH_H
#define EOG_PICA_DECAL_DEPTH_H
#include <cmath>
#include <algorithm>
/* Equivalent direction to reversed-depth polygon offset factor 2. Only
 * decals use this; ordinary world/viewmodel vertices remain byte-identical. */
static inline float pica_decal_slope(const float *a,const float *b,const float *c,int width,int height,bool mapped=false) {
  if(width<=0 || height<=0 || a[3]<=0 || b[3]<=0 || c[3]<=0)return 0;
  float ax=a[0]/a[3],ay=a[1]/a[3],az=mapped?-a[2]/a[3]:.5f*(1-a[2]/a[3]);
  float bx=b[0]/b[3]-ax,by=b[1]/b[3]-ay,bz=(mapped?-b[2]/b[3]:.5f*(1-b[2]/b[3]))-az;
  float cx=c[0]/c[3]-ax,cy=c[1]/c[3]-ay,cz=(mapped?-c[2]/c[3]:.5f*(1-c[2]/c[3]))-az;
  float area=bx*cy-by*cx;
  if(!std::isfinite(area) || std::fabs(area)<1e-10f)return 0;
  float dx=(bz*cy-cz*by)/area*2/width,dy=(bx*cz-cx*bz)/area*2/height;
  float slope=2*std::max(std::fabs(dx),std::fabs(dy));
  return std::isfinite(slope)?std::min(slope,1.f/4096):0;
}
#endif
