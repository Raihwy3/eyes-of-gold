#ifndef EOG_BOTTOM_MOCK_3DS_H
#define EOG_BOTTOM_MOCK_3DS_H
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
uint64_t osGetTime(void);
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;
typedef pthread_mutex_t LightLock;
static inline void LightLock_Init(LightLock *m){pthread_mutex_init(m,0);}
static inline void LightLock_Lock(LightLock *m){pthread_mutex_lock(m);}
static inline void LightLock_Unlock(LightLock *m){pthread_mutex_unlock(m);}
static inline int LightLock_TryLock(LightLock *m){return pthread_mutex_trylock(m);}
enum {GFX_BOTTOM=1,GFX_LEFT=0,GSP_RGB565_OES=2};
void gfxSetScreenFormat(int,int);
void gfxSetDoubleBuffering(int,bool);
u8 *gfxGetFramebuffer(int,int,u16 *,u16 *);
void GSPGPU_FlushDataCache(void *,u32);
void gfxScreenSwapBuffers(int,bool);
#endif
