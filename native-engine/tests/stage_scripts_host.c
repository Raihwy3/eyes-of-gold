/* Walk real retail local AI through the original engine's command-size parser. */
#include <ultra64.h>
#include "game/chrai.h"
#include "aicommands2.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef assert
#define assert(condition) do { if(!(condition)) { fprintf(stderr,"Stage script check failed at line %d\n",__LINE__);abort(); } } while(0)
void osSyncPrintf(const char *format,...) {(void)format;abort();}
static unsigned word(const unsigned char *p,int little) {
    return little?(unsigned)p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24:
        (unsigned)p[3]|(unsigned)p[2]<<8|(unsigned)p[1]<<16|(unsigned)p[0]<<24;
}
static unsigned char *read_file(const char *name,unsigned *size) {
    FILE *f=fopen(name,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);
    assert(n>=40 && n<1024*1024);*size=(unsigned)n;rewind(f);
    unsigned char *p=malloc(*size);assert(p && fread(p,1,*size,f)==*size);fclose(f);return p;
}
int main(int argc,char **argv) {
    assert(argc==4);unsigned bytes,other;
    unsigned char *retail=read_file(argv[1],&bytes),*native=read_file(argv[2],&other);
    assert(bytes==other);unsigned table=word(retail+20,0),lists=0,commands=0;
    assert(table==word(native+20,1) && table<bytes);
    for(unsigned row=table;;row+=8) {
        assert(row+8<=bytes);unsigned script=word(retail+row,0);
        assert(script==word(native+row,1));if(!script)break;
        assert(script<bytes);
        assert(word(retail+row+4,0)==word(native+row+4,1));
        unsigned offset=0;
        for(;;) {
            assert(script+offset<bytes);
            /* PRINT is the only variable-sized local record. Bound its NUL
             * before invoking the original parser, which expects valid data. */
            if(retail[script+offset]==AI_PRINT)
                assert(memchr(retail+script+offset+1,0,bytes-script-offset-1));
            int length=chraiitemsize(retail+script,offset);
            assert(length>0 && (unsigned)length<=bytes-script-offset);
            assert(!memcmp(retail+script+offset,native+script+offset,(unsigned)length));
            assert(length==chraiitemsize(native+script,offset));commands++;
            unsigned op=retail[script+offset];offset+=(unsigned)length;
            if(op==AI_EndList)break;
            assert(commands<100000);
        }
        lists++;assert(lists<1000);
    }
    if(!lists)assert(!memcmp(argv[3],"Ump_setup",9) && !commands);
    else assert(commands);
    printf("PASS original local AI parser: %s, %u lists / %u commands, exact bytecode and native list IDs/offsets\n",argv[3],lists,commands);
    free(retail);free(native);return 0;
}
