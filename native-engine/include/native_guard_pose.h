/* Game-thread-only caches of pure animation math. Keys contain source bytes,
 * never scratch addresses, so a recycled frame cannot reuse an old pose. */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
struct NativeGuardRotationEntry { uint64_t bits; unsigned key; coord3d value; };
static struct NativeGuardRotationEntry native_guard_rotations[512];
struct NativeGuardMatrixEntry { unsigned valid,key[3]; Mtxf value; };
static struct NativeGuardMatrixEntry native_guard_matrices[256];
static unsigned native_guard_rotation_hits,native_guard_rotation_misses,native_guard_matrix_hits,native_guard_matrix_misses;
static int native_guard_pose_disabled=-1;
static int native_guard_pose_enabled(void) {
    if(native_guard_pose_disabled<0)native_guard_pose_disabled=getenv("EOG_DISABLE_GUARD_POSE_CACHE")!=NULL;
    return !native_guard_pose_disabled;
}
static struct NativeGuardRotationEntry *native_guard_rotation_slot(unsigned offset,unsigned width,int flip,const u8 *stream,uint64_t *bits,unsigned *key) {
    if(!native_guard_pose_enabled() || width>16)return NULL;
    unsigned shift=offset&7,bytes=(shift+3*width+7)/8;
    uint64_t data=0;
    for(unsigned i=0;i<bytes;i++)data|=(uint64_t)stream[(offset>>3)+i]<<(8*i);
    *bits=data;*key=1+shift+(width<<3)+((flip!=0)<<8);
    unsigned h=(unsigned)data^(unsigned)(data>>32)^(*key*0x9e3779b9u);h^=h>>16;
    return &native_guard_rotations[h&511];
}
static void native_guard_local_matrix(coord3d *position,coord3d *rotation,Mtxf *out) {
    if(!native_guard_pose_enabled()){matrix_4x4_set_position_and_rotation_around_xyz(position,rotation,out);return;}
    unsigned key[3];memcpy(key,rotation,sizeof key);
    unsigned h=key[0]*0x9e3779b9u+key[1]*0x85ebca6bu+key[2];h^=h>>16;
    struct NativeGuardMatrixEntry *e=&native_guard_matrices[h&255];
    if(e->valid && !memcmp(key,e->key,sizeof key)){*out=e->value;native_guard_matrix_hits++;}
    else {
        matrix_4x4_set_rotation_around_xyz(rotation,out);
        memcpy(e->key,key,sizeof key);e->value=*out;e->valid=1;native_guard_matrix_misses++;
    }
    /* Translation is always live; cached matrix is the unscaled local rotation. */
    matrix_4x4_set_position(position,out);
}
void native_guard_pose_report(void) {
    sysLogPrintf(LOG_NOTE,"Guard pose cache: rotationsHit=%u decoded=%u localMatricesHit=%u built=%u bytes=%u",native_guard_rotation_hits,native_guard_rotation_misses,native_guard_matrix_hits,native_guard_matrix_misses,(unsigned)(sizeof native_guard_rotations+sizeof native_guard_matrices));
    native_guard_rotation_hits=native_guard_rotation_misses=native_guard_matrix_hits=native_guard_matrix_misses=0;
}
