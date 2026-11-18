#ifndef EOG_NATIVE_GPU_BOUNDS_H
#define EOG_NATIVE_GPU_BOUNDS_H
#include <cmath>
/* AABB support against homogeneous side planes. Keep intersecting boxes,
 * including those crossing W=0; never reject individual triangles here.
 * A relative guard covers float accumulation near the plane. */
static inline int native_gpu_bounds_outside(const float rows[4][4],const float lo[3],const float hi[3]) {
    for(unsigned axis=0;axis<2;axis++)for(int sign=-1;sign<=1;sign+=2) {
        float maximum=rows[3][3]+sign*rows[axis][3],magnitude=fabsf(maximum);
        for(unsigned i=0;i<3;i++) {
            float coefficient=rows[3][i]+sign*rows[axis][i];
            float term=coefficient*(coefficient>=0?hi[i]:lo[i]);
            maximum+=term;magnitude+=fabsf(term);
        }
        if(std::isfinite(maximum) && std::isfinite(magnitude) && maximum < -1.e-4f*(1.f+magnitude))return 1;
    }
    return 0;
}
#endif
