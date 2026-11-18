/* Exact offline ADPCM frame playback. No allocation, SD I/O or decoder work
 * on cache hits. This module has no libaudio or libctru ABI dependency. */
#include "native_audio_pcm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PCM_MAX_BYTES (24u*1024u*1024u)
static unsigned char *bank;
static uint32_t *index_table,book_count,frame_count,index_mask;
static uint32_t hits,misses;
static int book_lookup[128];
static uint32_t get32(const void *p){uint32_t n;memcpy(&n,p,4);return n;}
static uint32_t hash_bytes(uint32_t h,const unsigned char *p,unsigned bytes){while(bytes--)h=(h^*p++)*16777619u;return h;}
static unsigned char *frame_at(uint32_t i){return bank+book_count*260+i*48;}
int native_audio_pcm_mount(const char *directory) {
    if(bank)return 0; /* Startup-only; never invalidate pointers under workers. */
    char path[512];int n=snprintf(path,sizeof path,"%s/native-audio.pcm",directory);
    if(n<0 || (unsigned)n>=sizeof path)return 0;
    FILE *f=fopen(path,"rb");if(!f)return 0;
    unsigned char header[32];unsigned char *data=NULL;uint32_t *slots=NULL;int ok=0;
    if(fread(header,1,32,f)!=32 || memcmp(header,"EOGPCM2\0",8) || get32(header+8)!=2 || get32(header+28))goto done;
    uint32_t books=get32(header+12),frames=get32(header+16),bytes=get32(header+20);
    if(!books || books>1024 || !frames || frames>300000 || bytes>PCM_MAX_BYTES || (uint64_t)books*260+(uint64_t)frames*48!=bytes)goto done;
    data=malloc(bytes);if(!data)goto done;
    if(fread(data,1,bytes,f)!=bytes || fgetc(f)!=EOF || ferror(f) || hash_bytes(2166136261u,data,bytes)!=get32(header+24))goto done;
    for(unsigned i=0;i<books;i++){unsigned size=get32(data+i*260);if(!size || size>256 || size%32)goto done;}
    uint32_t capacity=1;while(capacity<frames*2)capacity*=2;
    slots=calloc(capacity,sizeof *slots);if(!slots)goto done;
    for(unsigned i=0;i<frames;i++) {
        unsigned char *r=data+books*260+i*48;unsigned id=r[0]|(unsigned)r[1]<<8;
        if(id>=books || (r[6]&15)>=get32(data+id*260)/32)goto done;
        uint32_t h=hash_bytes(2166136261u,r,15),slot=h&(capacity-1),steps=0;
        while(slots[slot]){if(++steps>=capacity)goto done;slot=(slot+1)&(capacity-1);}
        slots[slot]=i+1;
    }
    bank=data;index_table=slots;book_count=books;frame_count=frames;index_mask=capacity-1;ok=1;
done:
    fclose(f);if(!ok){free(data);free(slots);}return ok;
}
int native_audio_pcm_book(const void *coefficients,unsigned bytes) {
    if(!bank || !coefficients || !bytes || bytes>256)return -1;
    unsigned slot=((uintptr_t)coefficients>>4)&127;
    int known=book_lookup[slot]-1;
    if(known>=0) {
        const unsigned char *p=bank+(unsigned)known*260;
        if(get32(p)==bytes && !memcmp(p+4,coefficients,bytes))return known;
    }
    const unsigned char *p=bank;
    for(unsigned i=0;i<book_count;i++,p+=260)if(get32(p)==bytes && !memcmp(p+4,coefficients,bytes)){book_lookup[slot]=i+1;return (int)i;}
    return -1;
}
int native_audio_pcm_frame(int book,const unsigned char *frame,int16_t prev2,int16_t prev1,int16_t *output) {
    if(!bank || book<0 || (unsigned)book>=book_count || !frame || !output){misses++;return 0;}
    unsigned char key[15];key[0]=(unsigned)book&255;key[1]=(unsigned)book>>8;
    memcpy(key+2,&prev2,2);memcpy(key+4,&prev1,2);memcpy(key+6,frame,9);
    uint32_t slot=hash_bytes(2166136261u,key,15)&index_mask;
    for(unsigned steps=0;steps<=index_mask;steps++,slot=(slot+1)&index_mask) {
        unsigned n=index_table[slot];if(!n)break;
        const unsigned char *record=frame_at(n-1);
        if(!memcmp(record,key,15)){memcpy(output,record+15,32);hits++;return 1;}
    }
    misses++;return 0;
}
void native_audio_pcm_stats(uint32_t *h,uint32_t *m){if(h)*h=hits;if(m)*m=misses;}
