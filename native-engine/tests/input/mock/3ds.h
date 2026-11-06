#ifndef EOG_TEST_HID_H
#define EOG_TEST_HID_H
#include <stdint.h>
typedef uint32_t u32;
typedef struct {int16_t x,z,y;} angularRate;
#define SYSCLOCK_ARM11 268123480
#define R_SUCCEEDED(v) ((v)>=0)
#define R_FAILED(v) ((v)<0)
static inline int HIDUSER_EnableGyroscope(void){return 0;}
static inline int HIDUSER_DisableGyroscope(void){return 0;}
static inline int HIDUSER_GetGyroscopeRawToDpsCoefficient(float *c){*c=14.375f;return 0;}
extern angularRate test_gyro_rate;
static inline void hidGyroRead(angularRate *r){*r=test_gyro_rate;}
static inline uint64_t svcGetSystemTick(void){static uint64_t t;t+=SYSCLOCK_ARM11/60;return t;}
typedef int LightLock;
typedef struct {int dx,dy;} circlePosition;
typedef struct {unsigned px,py;} touchPosition;
enum {KEY_A=1,KEY_L=1<<9,KEY_Y=1<<11,KEY_ZR=1<<15,KEY_SELECT=1<<2,KEY_START=1<<3};
#define KEY_TOUCH (1u<<20)
static inline void hidTouchRead(touchPosition *p){p->px=p->py=0;}
static inline void LightLock_Init(LightLock *p){*p=0;}
static inline void LightLock_Lock(LightLock *p){(void)p;}
static inline void LightLock_Unlock(LightLock *p){(void)p;}
void hidScanInput(void);
u32 hidKeysHeld(void);
void hidCircleRead(circlePosition *p);
void hidCstickRead(circlePosition *p);
#endif
