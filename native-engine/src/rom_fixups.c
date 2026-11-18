/* Native-width frontend assets. Scalar/animation routines below are emitted
 * verbatim from the upstream port, separately from its fixed-address loader. */
#include <ultra64.h>
#include <bondtypes.h>
#include <string.h>
#include "romdata.h"
#include "system.h"
#include "rom_scalar_fixups.inc"

u32 romdataFontPcSize(const u8 *source,u32 n64_size)
{
    (void)source; /* May be a cart token, never a live CPU pointer on 3DS. */
    if (n64_size<169u*4u+94u*24u) sysFatalError("Native font segment is too small: %u",n64_size);
    return n64_size;
}
void romdataFixupFont(u8 *blob,u32 n64_size)
{
#if __SIZEOF_POINTER__ == 4
    _Static_assert(sizeof(struct fontchar)==24,"Original ARM32 glyph stride");
    _Static_assert(__builtin_offsetof(struct fontchar,pixeldata)==20,"Original glyph pointer slot");
    _Static_assert(__builtin_offsetof(struct font,chars)==676,"Original font header");
#endif
    romdataFontPcSize(NULL,n64_size);
    /* On ARM32 the original 169-word kerning table and 94 six-word glyphs
     * already have native offsets/strides. Pixel data stays in place. */
    for (u32 i=0;i<169u+94u*6u;++i) {
        u32 value;
        memcpy(&value,blob+4*i,4);
        value=romdataBswap32(value);
        memcpy(blob+4*i,&value,4);
    }
}
