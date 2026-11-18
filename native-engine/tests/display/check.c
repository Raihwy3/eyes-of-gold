#include <3ds.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_display.h"
/* Reproduce consoleInit's actual format and single-buffer state. */
static GSPGPU_FramebufferFormat format=GSP_RGB565_OES;
static bool double_buffer;
static unsigned back,swaps,waits,flushes;
static unsigned char buffers[2][2][4*2*3];
void gfxSet3D(bool enable){assert(enable);}
void gfxSetScreenFormat(gfxScreen_t screen,GSPGPU_FramebufferFormat f){assert(screen==GFX_TOP);format=f;}
GSPGPU_FramebufferFormat gfxGetScreenFormat(gfxScreen_t screen){assert(screen==GFX_TOP);return format;}
void gfxSetDoubleBuffering(gfxScreen_t screen,bool enable){assert(screen==GFX_TOP);double_buffer=enable;}
u8 *gfxGetFramebuffer(gfxScreen_t screen,gfx3dSide_t side,u16 *w,u16 *h){
 assert(screen==GFX_TOP && (side==GFX_LEFT || side==GFX_RIGHT));assert(format==GSP_BGR8_OES && double_buffer);
 *w=4;*h=2;return buffers[back][side];
}
void gfxFlushBuffers(void){++flushes;}
void gfxScreenSwapBuffers(gfxScreen_t screen,bool stereo){
 assert(screen==GFX_TOP && stereo && double_buffer);assert(flushes==swaps+1);
 for(unsigned i=0;i<sizeof buffers[back];++i)assert(!((unsigned char *)buffers[back])[i]);
 back^=1;++swaps;
}
void gspWaitForVBlank(void){assert(swaps==waits+1);++waits;}
int main(void){
 memset(buffers,0xa5,sizeof buffers);
 unsigned flags=native_display_prepare();
 assert(format==GSP_BGR8_OES && double_buffer);
 assert(((flags>>12)&7)==format);
 assert(swaps==2 && waits==2 && flushes==2);
 for(unsigned i=0;i<sizeof buffers;++i)assert(!((unsigned char *)buffers)[i]);
 puts("PASS console RGB565/single-buffer -> RGB8/double-buffer handoff");
 puts("PASS both framebuffers cleared and published before GPU output");
 puts("PASS display transfer format matches the actual LCD format");
}
