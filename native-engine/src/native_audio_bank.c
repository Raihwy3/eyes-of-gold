/* Retail bank records retain their 32-bit offsets on ARM. Convert scalars,
 * once per shared node, then let the original alBnkfNew relocate pointers. */
#include "native_audio.h"
#include <stdlib.h>
#include <string.h>
typedef struct {unsigned char *p,*seen;uint32_t n;} Bank;
static int range(Bank *b,uint32_t o,uint32_t n) {return o<=b->n && n<=b->n-o;}
static uint32_t word(Bank *b,uint32_t o) {
    unsigned char *p=b->p+o;uint32_t v=(uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];
    memcpy(p,&v,4);return v;
}
static uint16_t half(Bank *b,uint32_t o) {
    unsigned char *p=b->p+o;uint16_t v=(p[0]<<8)|p[1];memcpy(p,&v,2);return v;
}
enum {FILE_NODE=1,BANK_NODE,INST_NODE,SOUND_NODE,WAVE_NODE,ENV_NODE,KEY_NODE,LOOP_NODE,RAW_LOOP_NODE,BOOK_NODE};
static int node(Bank *b,uint32_t o,unsigned type) {
    static const unsigned minimum[]={0,4,12,16,16,20,16,6,44,12,8};
    if (!o && type!=FILE_NODE) return 1;
    if ((o&3) || !range(b,o,minimum[type])) return 0;
    if (b->seen[o/4]) return b->seen[o/4]==type;
    b->seen[o/4]=type;
    uint32_t count=0,base=0,child=0;
    switch(type) {
    case FILE_NODE:
        if (half(b,o)!=0x4231) return 0;
        count=half(b,o+2);base=4;child=BANK_NODE;break;
    case BANK_NODE:
        count=half(b,o);base=12;child=INST_NODE;
        if (b->p[o+2]) return 0;
        word(b,o+4);
        if (!node(b,word(b,o+8),INST_NODE)) return 0;
        break;
    case INST_NODE:
        if (b->p[o+3]) return 0;
        half(b,o+12);count=half(b,o+14);base=16;child=SOUND_NODE;break;
    case SOUND_NODE:
        if (b->p[o+14]) return 0;
        return node(b,word(b,o),ENV_NODE) && node(b,word(b,o+4),KEY_NODE) && node(b,word(b,o+8),WAVE_NODE);
    case WAVE_NODE:
        word(b,o);word(b,o+4);
        if (b->p[o+9]) return 0;
        if (b->p[o+8]==0)
            return node(b,word(b,o+12),LOOP_NODE) && node(b,word(b,o+16),BOOK_NODE);
        if (b->p[o+8]==1) return node(b,word(b,o+12),RAW_LOOP_NODE);
        return 0;
    case ENV_NODE:case RAW_LOOP_NODE:
        word(b,o);word(b,o+4);word(b,o+8);return 1;
    case KEY_NODE:return 1;
    case LOOP_NODE:
        word(b,o);word(b,o+4);word(b,o+8);
        for(unsigned i=0;i<16;i++) half(b,o+12+2*i);
        return 1;
    case BOOK_NODE: {
        uint32_t order=word(b,o),predictors=word(b,o+4);
        if (!order || order>16 || !predictors || predictors>128) return 0;
        uint32_t n=8*order*predictors;
        if (!range(b,o+8,2*n)) return 0;
        for(uint32_t i=0;i<n;i++)half(b,o+8+2*i);
        return 1;
    }
    default:return 0;
    }
    if (count>32767 || !range(b,o+base,4*count)) return 0;
    for(uint32_t i=0;i<count;i++) if(!node(b,word(b,o+base+4*i),child))return 0;
    return 1;
}
int native_audio_bank_convert(void *blob,uint32_t bytes) {
    if (!blob || bytes<4) return 0;
    Bank b={blob,calloc((bytes+3)/4,1),bytes};
    if(!b.seen)return 0;
    int ok=node(&b,0,FILE_NODE);free(b.seen);return ok;
}
/* A single retail instrument uses raw PCM. Its sample payload also needs
 * endian conversion, at the audio DMA boundary, without modifying the ROM. */
static struct {uint32_t first,last;} raw_ranges[64];
static unsigned raw_count;
static uint32_t le32(const unsigned char *p) {uint32_t n;memcpy(&n,p,4);return n;}
static uint16_t le16(const unsigned char *p) {uint16_t n;memcpy(&n,p,2);return n;}
int native_audio_register_raw(const void *blob,uint32_t table) {
    const unsigned char *p=blob;
    for(unsigned b=0;b<le16(p+2);b++) {
        uint32_t bank=le32(p+4+4*b);
        for(unsigned i=0;i<=le16(p+bank);i++) {
            uint32_t inst=le32(p+bank+8+4*i);
            if(!inst)continue;
            for(unsigned s=0;s<le16(p+inst+14);s++) {
                uint32_t sound=le32(p+inst+16+4*s),wave=le32(p+sound+8);
                if(p[wave+8]!=1)continue;
                uint64_t first=(uint64_t)table+le32(p+wave),last=first+le32(p+wave+4);
                if(last>UINT32_MAX || (first&1) || (last&1))return 0;
                unsigned j;
                for(j=0;j<raw_count;j++)if(raw_ranges[j].first==first && raw_ranges[j].last==last)break;
                if(j<raw_count)continue;
                if(raw_count==64)return 0;
                raw_ranges[raw_count].first=first;raw_ranges[raw_count++].last=last;
            }
        }
    }
    return 1;
}
void native_audio_swap_samples(uint32_t address,void *data,uint32_t bytes) {
    unsigned char *p=data;
    for(unsigned i=0;i<raw_count;i++) {
        uint64_t end=(uint64_t)address+bytes;
        uint32_t first=address>raw_ranges[i].first?address:raw_ranges[i].first;
        uint32_t last=end<raw_ranges[i].last?(uint32_t)end:raw_ranges[i].last;
        if(first>=last)continue;
        for(uint32_t a=first;a+1<last;a+=2) {
            unsigned k=a-address;unsigned char c=p[k];p[k]=p[k+1];p[k+1]=c;
        }
    }
}
