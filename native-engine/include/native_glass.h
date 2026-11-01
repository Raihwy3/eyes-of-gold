#ifndef EOG_NATIVE_GLASS_H
#define EOG_NATIVE_GLASS_H
/* Explicit pure-glass prop scope; doors, world and metal weapons are excluded. */
/* Retain distant opacity: those panes may close a visibility portal. */
static inline unsigned native_glass_alpha(unsigned authored){return authored<128?128:authored>255?255:authored;}
/* Original reflections use reduced mip detail at mid distance and a flat
 * pane at far distance. Simple is already the cheapest pane material. */
static inline int native_glass_lod_simple(int simple,unsigned tier){return simple || tier>=2;}
#define NATIVE_GLASS_DETAIL_COMMAND 0x4c
#define native_glass_detail_command(pkt,simple,tier) gDma0p((pkt),NATIVE_GLASS_DETAIL_COMMAND,0,((simple)?1:0)|(((tier)&3)<<1))
#define NATIVE_GLASS_SCOPE 0x080000u
#define native_glass_scope(pkt,on) \
    gDma0p((pkt),0x3a,(on)?NATIVE_GLASS_SCOPE:0,((~NATIVE_GLASS_SCOPE)&0xffffffu))
#endif
