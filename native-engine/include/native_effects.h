#ifndef EOG_NATIVE_EFFECTS_H
#define EOG_NATIVE_EFFECTS_H
struct coord3d;
#ifdef __cplusplus
extern "C" {
#endif
/* Game-thread cosmetic policy only: -1 beyond fog, otherwise tiers 0..3. */
int native_effects_tier(const struct coord3d *pos,float radius);
int native_effects_spawn(const struct coord3d *pos,float radius);
int native_effects_keep(int tier,unsigned index);
int native_effects_beam(const struct coord3d *pos,const struct coord3d *delta,float length);
void native_effects_report(void);
#ifdef __cplusplus
}
#endif
/* Separate texture-only scope; never enables scenery topology reduction. */
#define NATIVE_EFFECT_SCOPE_COMMAND 0x4e
#define native_effect_scope(pkt,tier,on) gDma0p((pkt),NATIVE_EFFECT_SCOPE_COMMAND,0,(on)?((tier)+1):0)
#endif
