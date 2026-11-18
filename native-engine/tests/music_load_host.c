/* Execute all three real music-loading functions with retail streams.
 * Sequence-player callbacks stop at the audio boundary; decompression is real. */
#include <ultra64.h>
#include "music.h"
#include "ramrom.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#undef assert
#define assert(x) ((x)?(void)0:(fprintf(stderr,"Assertion failed: %s at %d\n",#x,__LINE__),abort()))
s8 g_sndBootswitchSound;
extern RareALSeqBankFile *g_musicDataTable;
extern u16 g_musicTrackLength[],g_musicTrackCompressedLength[];
extern u8 *g_musicXTrack1SeqData,*g_musicXTrack2SeqData,*g_musicXTrack3SeqData;
extern ALCSPlayer *g_musicXTrack1SeqPlayer,*g_musicXTrack2SeqPlayer,*g_musicXTrack3SeqPlayer;
static u8 *cart,*expected;
static size_t cart_size,expected_size;
static int checked,played,yields,polls,stuck;
static ALCSPlayer *stop_pending;
static uint64_t virtual_us;
uint64_t sysGetMicroseconds(void) {return virtual_us;}
int videoQuitRequested(void) {return 0;}
void sysSleep(uint32_t us) {
    assert(us==1000 && stop_pending);
    virtual_us+=us;++yields;if(!stuck){stop_pending->state=0;stop_pending=NULL;polls=0;}
}
extern s32 g_musicXTrack1CurrentTrackNum,g_musicXTrack2CurrentTrackNum,g_musicXTrack3CurrentTrackNum;
void sysLogPrintf(int level,const char *fmt,...) {(void)level;(void)fmt;}
void sysFatalError(const char *fmt,...) {va_list a;va_start(a,fmt);vfprintf(stderr,fmt,a);va_end(a);abort();}
void romCopy(void *dst,void *src,u32 size) {
    uintptr_t offset=(uintptr_t)src-0x10000000u;
    assert(offset<=cart_size && size<=cart_size-offset);memcpy(dst,cart+offset,size);
}
s32 alCSPGetState(ALCSPlayer *p) {assert(++polls<(stuck?10000:100));return p->state;}
void alCSPStop(ALCSPlayer *p) {assert(!stop_pending);stop_pending=p;polls=0;}
void alCSPPlay(ALCSPlayer *p) {assert(p);played++;}
void alCSPSetVol(ALCSPlayer *p,s16 v) {assert(p);(void)v;}
void alCSPSetSeq(ALCSPlayer *p,ALCSeq *s) {assert(p && s);}
void alCSeqNew(ALCSeq *seq,u8 *data) {
    assert(seq && !memcmp(data,expected,expected_size));checked++;
}
static u16 be16(const u8 *p) {return p[0]*256u+p[1];}
static u32 be32(const u8 *p) {return (u32)p[0]<<24|(u32)p[1]<<16|be16(p+2);}
static void read_exact(FILE *f,void *p,size_t n) {assert(fread(p,1,n,f)==n);}
int main(int argc,char **argv) {
    assert(argc==3 || argc==4);stuck=argc==4;FILE *f=fopen(argv[1],"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));cart_size=ftell(f);rewind(f);
    cart=malloc(cart_size);read_exact(f,cart,cart_size);fclose(f);
    f=fopen(argv[2],"rb");assert(f);
    const u8 *table=cart+0x419790;unsigned n=be16(table);assert(n==63);
    g_musicDataTable=calloc(1,4+8*n);g_musicDataTable->seqCount=n;
    static ALCSPlayer players[3];
    static u8 buffers[3][8192];
    g_musicXTrack1SeqPlayer=&players[0];g_musicXTrack2SeqPlayer=&players[1];g_musicXTrack3SeqPlayer=&players[2];
    g_musicXTrack1SeqData=buffers[0];g_musicXTrack2SeqData=buffers[1];g_musicXTrack3SeqData=buffers[2];
    for(unsigned i=0;i<n;i++) {
        const u8 *e=table+4+8*i;
        g_musicDataTable->seqArray[i].address=0x10419790u+be32(e);
        g_musicTrackLength[i]=(be16(e+4)+1)&~1u;g_musicTrackCompressedLength[i]=be16(e+6);
    }
    for(unsigned i=0;i<n;i++) {
        expected_size=be16(table+4+8*i+4);assert(expected_size<sizeof buffers[0]-128);
        expected=malloc(expected_size);read_exact(f,expected,expected_size);
        /* Expected fixture is independent Python zlib output; the original
         * loading function also converts its 17-word cseq header. */
        for(unsigned j=0;j<68;j+=4) {u32 v=be32(expected+j);memcpy(expected+j,&v,4);}
        g_musicXTrack1CurrentTrackNum=g_musicXTrack2CurrentTrackNum=g_musicXTrack3CurrentTrackNum=1;
        for(unsigned p=0;p<3;p++)players[p].state=1;
        musicTrack1Play(i);musicTrack2Play(i);musicTrack3Play(i);free(expected);
    }
    assert(checked==189 && played==189 && yields==189);assert(fgetc(f)==EOF);fclose(f);
    free(g_musicDataTable);free(cart);
    puts("PASS all 63 retail sequences through each original music player: 189 exact loads and cooperative audio-stop handoffs");
}
