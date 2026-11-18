#ifndef EOG_NATIVE_VIEWMODEL_H
#define EOG_NATIVE_VIEWMODEL_H
#define NATIVE_VIEWMODEL_SCOPE 0x400000u
/* Preserve every other extra geometry flag, including HUD anchoring. */
#define native_viewmodel_scope(pkt,on) \
    gDma0p((pkt),0x3a,(on)?NATIVE_VIEWMODEL_SCOPE:0,((~NATIVE_VIEWMODEL_SCOPE)&0xffffffu))
#endif
