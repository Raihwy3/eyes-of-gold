#ifndef EOG_NATIVE_DETAIL_H
#define EOG_NATIVE_DETAIL_H
/* Distance is world-space engine units, with model radius retained at the
 * boundary. Only render callers use this; AI/rooms/collision keep retail range. */
static inline float native_detail_lod_scale(int strength) {
    return strength==1?2.f:strength==2?4.f:strength==3?8.f:1.f;
}
static inline float native_detail_distance(int choice) {
    return choice==1?4000.f:choice==2?2200.f:choice==3?1200.f:0.f;
}
static inline float native_detail_fade(float depth,float radius,int choice) {
    float end=native_detail_distance(choice);
    if(!end)return 1.f;
    if(radius<0)radius=0;
    float edge=depth-radius,start=end*.75f;
    if(edge<=start)return 1.f;
    if(edge>=end)return 0.f;
    return (end-edge)/(end-start);
}
/* Full detail plus three reduced distance tiers. Larger authored-LOD strength moves both boundaries in.
 * Zero/Original keeps exact retail geometry and texture selection. */
static inline int native_detail_tier(float nearest_depth,int strength) {
    if(strength<=0 || !(nearest_depth>0))return 0;
    float distance=nearest_depth*native_detail_lod_scale(strength);
    return distance>=6000.f?3:distance>=3000.f?2:distance>=1200.f?1:0;
}
#endif
