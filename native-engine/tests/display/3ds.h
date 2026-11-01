/* Only the display API boundary used by display_3ds.c. */
#ifndef TEST_3DS_H
#define TEST_3DS_H
#include <stdbool.h>
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef enum { GFX_TOP, GFX_BOTTOM } gfxScreen_t;
typedef enum { GFX_LEFT, GFX_RIGHT } gfx3dSide_t;
typedef enum { GSP_BGR8_OES=1, GSP_RGB565_OES=2 } GSPGPU_FramebufferFormat;
#define GX_TRANSFER_FMT_RGBA8 0
#define GX_TRANSFER_IN_FORMAT(v) (((v)&7)<<8)
#define GX_TRANSFER_OUT_FORMAT(v) (((v)&7)<<12)
void gfxSet3D(bool);
void gfxSetScreenFormat(gfxScreen_t,GSPGPU_FramebufferFormat);
GSPGPU_FramebufferFormat gfxGetScreenFormat(gfxScreen_t);
void gfxSetDoubleBuffering(gfxScreen_t,bool);
u8 *gfxGetFramebuffer(gfxScreen_t,gfx3dSide_t,u16 *,u16 *);
void gfxFlushBuffers(void);
void gfxScreenSwapBuffers(gfxScreen_t,bool);
void gspWaitForVBlank(void);
#endif
