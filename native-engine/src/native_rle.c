/* Validate the retail BE image before the original decoder can write anything. */
#include "native_rle.h"
#include <stdint.h>
#include <string.h>
int native_rle_prepare(unsigned char *p,size_t n,size_t capacity) {
    if (!p || n<10) return 0;
    uint16_t w=(p[0]<<8)|p[1],h=(p[2]<<8)|p[3];
    size_t remaining=(size_t)w*h;
    if (!w || !h || remaining>capacity) return 0;
    for (size_t i=10;remaining;i+=2) {
        if (i+2>n || !p[i] || p[i]>remaining) return 0;
        remaining-=p[i];
    }
    memcpy(p,&w,2);memcpy(p+2,&h,2);
    return 1;
}
