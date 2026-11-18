#include <ultra64.h>
#include <bondtypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "matrixmath.h"
#include "system.h"
#include "pose-functions.h"
#include "native_test_assert.h"
static unsigned rng=1234567;
static unsigned next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
void sysLogPrintf(enum LogLevel level,const char *format,...){(void)level;(void)format;}
int main(void) {
    ModelJoint joints[8]={0};ModelSkeleton skeleton={0};skeleton.Joints=joints;ModelAnimation anim={0};
    u8 stream[512];coord3d a,b;
    for(unsigned width=0;width<=16;width++)for(unsigned flip=0;flip<2;flip++)for(unsigned joint=0;joint<8;joint++) {
        anim.unk06=width;skeleton.Joints[joint].mtxA=joint;skeleton.Joints[joint].mtxB=7-joint;
        for(unsigned mutation=0;mutation<50;mutation++) {
            for(unsigned i=0;i<sizeof stream;i++)stream[i]=next();
            for(unsigned repeat=0;repeat<3;repeat++) {
                reference_decode(joint,flip,&skeleton,&anim,stream,&a);
                sub_GAME_7F06DEC0(joint,flip,&skeleton,&anim,stream,&b);
                assert(!memcmp(&a,&b,sizeof a));
            }
        }
    }
    /* Identical bytes at different addresses and changed metadata at one address. */
    u8 copy[512];memcpy(copy,stream,sizeof copy);
    for(unsigned i=0;i<10000;i++) {
        unsigned joint=next()%8,flip=next()%2;anim.unk06=next()%17;
        reference_decode(joint,flip,&skeleton,&anim,copy,&a);
        sub_GAME_7F06DEC0(joint,flip,&skeleton,&anim,copy,&b);assert(!memcmp(&a,&b,sizeof a));
        coord3d rotation={{(int)(next()%100)*.01f,(int)(next()%100)*.02f,(int)(next()%100)*.03f}};
        for(unsigned repeat=0;repeat<3;repeat++) {
            coord3d pos={{(float)(int)next(),(float)(int)next(),(float)(int)next()}};
            Mtxf expected,actual;memset(&actual,0xa5,sizeof actual);
            matrix_4x4_set_position_and_rotation_around_xyz(&pos,&rotation,&expected);
            native_guard_local_matrix(&pos,&rotation,&actual);assert(!memcmp(&expected,&actual,sizeof expected));
        }
    }
    if(native_guard_pose_enabled()){assert(native_guard_rotation_hits && native_guard_rotation_misses && native_guard_matrix_hits && native_guard_matrix_misses);}
    else assert(!native_guard_rotation_hits && !native_guard_matrix_hits);
    printf("PASS actual guard pose/reference (%s): all bit widths, mirrored joints, scratch reuse/mutation, content sharing, exact local matrices and live translation; cache bytes=%u\n",native_guard_pose_enabled()?"cached":"reference",(unsigned)(sizeof native_guard_rotations+sizeof native_guard_matrices));
}
