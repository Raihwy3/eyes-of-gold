#include "native_audio_pcm.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native-adpcm-reference.h"
#include "native-adpcm-hook.h"
static uint32_t read32(const void *p){uint32_t x;memcpy(&x,p,4);return x;}
int main(int argc,char **argv) {
    if(argc==3){assert(native_audio_pcm_mount(argv[1])==atoi(argv[2]));return 0;}
    assert(argc==2);
    char path[1024];snprintf(path,sizeof path,"%s/native-audio.pcm",argv[1]);
    FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long bytes=ftell(f);rewind(f);
    unsigned char *data=malloc(bytes);assert(data && fread(data,1,bytes,f)==(size_t)bytes);fclose(f);
    assert(!native_audio_pcm_mount("/this/path/does/not/exist"));
    assert(native_audio_pcm_mount(argv[1]));assert(!native_audio_pcm_mount(argv[1]));
    unsigned books=read32(data+12),frames=read32(data+16);assert(books && frames>100000);
    unsigned char *b=data+32,*records=b+books*260;
    int *ids=malloc(books*sizeof *ids);assert(ids);
    for(unsigned i=0;i<books;i++) {
        ids[i]=native_audio_pcm_book(b+i*260+4,read32(b+i*260));assert(ids[i]==(int)i);
        assert(native_audio_pcm_book(b+i*260+4,read32(b+i*260))==ids[i]);
    }
    for(unsigned i=0;i<frames;i++) {
        unsigned char *r=records+i*48;unsigned id=r[0]|r[1]<<8;int16_t prev2,prev1;
        memcpy(&prev2,r+2,2);memcpy(&prev1,r+4,2);int16_t actual[16],reference[16];
        original_frame(r+6,(const int16_t *)(b+id*260+4),prev2,prev1,reference);
        assert(!memcmp(reference,r+15,32));
        assert(native_audio_pcm_frame(ids[id],r+6,prev2,prev1,actual));
        assert(!memcmp(actual,reference,32));
        if(i%100==0) {
            unsigned char invalid[9];memcpy(invalid,r+6,9);invalid[0]|=15;
            memset(actual,0xa5,sizeof actual);int16_t copy[16];memcpy(copy,actual,32);
            assert(!native_audio_pcm_frame(ids[id],invalid,prev2,prev1,actual));assert(!memcmp(actual,copy,32));
        }
    }
    assert(native_audio_pcm_book(NULL,32)==-1 && native_audio_pcm_book(b,257)==-1);
    int16_t result[16];assert(!native_audio_pcm_frame(-1,records,0,0,result));
    uint32_t hits,misses;native_audio_pcm_stats(&hits,&misses);assert(hits==frames && misses>1000);
    printf("PASS all-bank PCM/reference: %u predictor books, %u PCM frames exactly match original C decoder; loop histories, repeated book binding and unknown-frame fallback preserve state\n",books,frames);
    for(unsigned i=0;i<frames;i+=101) {
        unsigned char *r=records+i*48;unsigned id=r[0]|r[1]<<8;int16_t prev2,prev1;
        memcpy(&prev2,r+2,2);memcpy(&prev1,r+4,2);
        const int16_t *book=(const int16_t *)(b+id*260+4);
        memset(sAdpcmTable,0,sizeof sAdpcmTable);memcpy(sAdpcmTable,book,read32(b+id*260));
        native_pcm_book=ids[id];
        ADPCM_STATE state,loop;for(unsigned j=0;j<16;j++)state[j]=loop[j]=(int16_t)(j*319);
        state[14]=loop[14]=prev2;state[15]=loop[15]=prev1;
        sCurrentState=&state;sAdpcmLoopState=&loop;sCtx.in=0;sCtx.out=128;sCtx.count=64;
        memcpy(DMEM_U8(0),r+6,9);memcpy(DMEM_U8(9),r+6,9);
        int16_t expected[32];original_frame(r+6,book,prev2,prev1,expected);
        original_frame(r+6,book,expected[14],expected[15],expected+16);
        aADPCMdecImpl(0,1);
        assert(!memcmp(DMEM_S16(128)+16,expected,64) && !memcmp(state,expected+16,32));
        // A_LOOP must use the authored loop history; other voice state differs.
        memset(state,0x37,sizeof state);sCtx.count=32;aADPCMdecImpl(2,1);
        assert(!memcmp(DMEM_S16(128)+16,expected,32) && !memcmp(state,expected,32));
        // INIT and cache-disabled fallback share the original zero history.
        original_frame(r+6,book,0,0,expected);aADPCMdecImpl(1,1);
        assert(!memcmp(DMEM_S16(128)+16,expected,32));
        native_pcm_book=-1;aADPCMdecImpl(1,1);assert(!memcmp(DMEM_S16(128)+16,expected,32));
    }
    puts("PASS actual mixer PCM hook: multi-frame mixed hit/fallback, INIT/CONTINUE/LOOP and final predictor state match original output");
    free(ids);free(data);
}
