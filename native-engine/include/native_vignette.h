#ifndef EOG_NATIVE_VIGNETTE_H
#define EOG_NATIVE_VIGNETTE_H
/* Snapshot bits: live gameplay, crouch (including auto-crouch), aim/zoom. */
static inline int native_vignette_active(int mode,unsigned state) {
    if(mode==0)return 1;
    if(!(state&1))return 0;
    return mode==1?(state&2)!=0:mode==2?(state&4)!=0:mode==3?(state&6)!=0:0;
}
#endif
