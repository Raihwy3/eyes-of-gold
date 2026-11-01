/* consoleInit changes both pixel format and buffering. Restore the LCD output
 * contract before connecting a Citro3D render target; bottom remains a console.
 */
#include "native_display.h"
#include <3ds.h>
#include <string.h>
uint32_t native_display_prepare(void) {
  gfxSet3D(true);
  gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
  gfxSetDoubleBuffering(GFX_TOP, true);
  /* Publish the new format before rendering, and clear both buffers so the
   * console's old RGB565 bytes cannot be displayed as RGB8 during startup. */
  for (unsigned i = 0; i < 2; ++i) {
    u16 width, height;
    u8 *pixels = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &width, &height);
    memset(pixels, 0, (size_t)width * height * 3);
    pixels=gfxGetFramebuffer(GFX_TOP,GFX_RIGHT,&width,&height);
    memset(pixels,0,(size_t)width*height*3);
    gfxFlushBuffers();
    gfxScreenSwapBuffers(GFX_TOP, true);
    gspWaitForVBlank();
  }
  return GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
         GX_TRANSFER_OUT_FORMAT(gfxGetScreenFormat(GFX_TOP));
}
