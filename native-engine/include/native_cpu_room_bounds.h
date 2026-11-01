#ifndef EOG_NATIVE_CPU_ROOM_BOUNDS_H
#define EOG_NATIVE_CPU_ROOM_BOUNDS_H
#include <cmath>
#include <algorithm>
/* Loose independent intervals deliberately overestimate a vertex block.
 * No reassociated projection coefficients are used for actual rendering.
 * Near/behind-camera and uncertain blocks must use the CPU reference. */
static inline bool native_cpu_room_outside(const float mp[4][4],const float lo[3],const float hi[3],
        bool framebuffer,float offset,float scale,float aspect,float fog_end=0) {
    for(unsigned i=0;i<3;i++)if(!(lo[i]<=hi[i]) || !std::isfinite(lo[i]) || !std::isfinite(hi[i]))return false;
    float lower[4],upper[4];
    for(unsigned row=0;row<4;row++) {
        float low=mp[3][row],high=low,magnitude=std::fabs(low);
        for(unsigned i=0;i<3;i++) {
            float c=mp[i][row];
            low+=c*(c>=0?lo[i]:hi[i]);high+=c*(c>=0?hi[i]:lo[i]);
            magnitude+=std::fabs(c)*std::max(std::fabs(lo[i]),std::fabs(hi[i]));
        }
        // A generous 1% guard also covers cancellation and bound rounding.
        float guard=.01f*(1.f+magnitude);
        lower[row]=low-guard;upper[row]=high+guard;
        if(!std::isfinite(lower[row]) || !std::isfinite(upper[row]))return false;
    }
    if(lower[3]<=.001f || lower[2]+lower[3]<=.001f)return false;
    if(!framebuffer) {
        if(!(scale>0 && aspect>0) || !std::isfinite(offset))return false;
        lower[0]=(lower[0]+offset*(offset>=0?lower[3]:upper[3]))*scale/aspect;
        upper[0]=(upper[0]+offset*(offset>=0?upper[3]:lower[3]))*scale/aspect;
        float guard=.01f*(1.f+std::max(std::fabs(lower[0]),std::fabs(upper[0])));
        lower[0]-=guard;upper[0]+=guard;
        if(!std::isfinite(lower[0]) || !std::isfinite(upper[0]))return false;
    }
    for(unsigned row=0;row<2;row++)
        if(upper[row]<-upper[3] || lower[row]>upper[3])return true;
    if(lower[2]>upper[3])return true;
    if(fog_end>0 && std::isfinite(fog_end) && lower[2]>fog_end*upper[3])return true;
    return false;
}
#endif
