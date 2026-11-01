#ifndef EOG_NATIVE_AA_H
#define EOG_NATIVE_AA_H
/* N64 RDP AA_EN only. Coverage/alpha, blending and depth flags stay intact.
 * Keep the raw display-list state so turning AA back on restores each
 * material's own request rather than forcing AA onto every surface. */
static inline unsigned native_aa_render_mode(unsigned mode,int enabled) {
    return enabled?mode:mode&~0x8u;
}
#endif
