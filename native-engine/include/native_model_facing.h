#ifndef EOG_NATIVE_MODEL_FACING_H
#define EOG_NATIVE_MODEL_FACING_H
#include <cmath>
#include <algorithm>
/* The homogeneous screen winding is a model-space surface plane evaluated
 * against the cofactors of the projection's X/Y/W rows (Cauchy-Binet).
 * This avoids resolving clip XY for every corner merely to reject a face. */
class NativeModelFacing {
    float eye[4]={};bool valid=false;
public:
    NativeModelFacing()=default;
    explicit NativeModelFacing(const float rows[4][4]){set(rows);}
    void set(const float rows[4][4]) {
        double cofactors[4]={},magnitude=0;
        for(unsigned omitted=0;omitted<4;omitted++) {
            unsigned columns[3],n=0;for(unsigned c=0;c<4;c++)if(c!=omitted)columns[n++]=c;
            unsigned a=columns[0],b=columns[1],c=columns[2];
            double determinant=double(rows[0][a])*(double(rows[1][b])*rows[3][c]-double(rows[1][c])*rows[3][b])
                -double(rows[0][b])*(double(rows[1][a])*rows[3][c]-double(rows[1][c])*rows[3][a])
                +double(rows[0][c])*(double(rows[1][a])*rows[3][b]-double(rows[1][b])*rows[3][a]);
            cofactors[omitted]=omitted&1?-determinant:determinant;
            magnitude=std::max(magnitude,std::fabs(cofactors[omitted]));
        }
        valid=std::isfinite(magnitude) && magnitude>0;
        if(valid)for(unsigned i=0;i<4;i++)eye[i]=float(cofactors[i]/magnitude);
    }
    /* Same sign convention as native_triangle_facing. Zero means uncertain:
     * the caller must use the original loaded-coordinate predicate. */
    int sign(const float *a,const float *b,const float *c)const {
        if(!valid)return 0;
        float u[3],v[3];for(unsigned i=0;i<3;i++){u[i]=b[i]-a[i];v[i]=c[i]-a[i];}
        float plane[4]={u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0],0};
        plane[3]=-(plane[0]*a[0]+plane[1]*a[1]+plane[2]*a[2]);
        // Bound the uncancelled products as well as the final dot product.
        // Common face tests use ARM hardware floats, not software doubles.
        float bound[4]={std::fabs(u[1]*v[2])+std::fabs(u[2]*v[1]),
            std::fabs(u[2]*v[0])+std::fabs(u[0]*v[2]),
            std::fabs(u[0]*v[1])+std::fabs(u[1]*v[0]),0};
        for(unsigned i=0;i<3;i++)bound[3]+=bound[i]*std::fabs(a[i]);
        float result=0,magnitude=0;
        for(unsigned i=0;i<4;i++){result+=plane[i]*eye[i];magnitude+=bound[i]*std::fabs(eye[i]);}
        // Conservative around camera/face-plane crossings and cancellation.
        // The fallback also preserves the reference's float vertex transform.
        if(!std::isfinite(result) || std::fabs(result)<=1.e-4f*magnitude)return 0;
        return result>0?-1:1;
    }
};
#endif
