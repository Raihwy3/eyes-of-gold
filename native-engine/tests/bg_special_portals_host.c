/* The linker may insert padding between the ROM table and its old end symbol. */
#include <ultra64.h>
#include "game/bg.h"
#include "native_test_assert.h"
#include <string.h>
#include <sanitizer/asan_interface.h>
static struct {
    u8 table[20];
    u8 linker_padding[4];
    s32 current_room;
    u8 following_data[64];
} storage __attribute__((aligned(8)));
#define specialportalarray storage.table
#define g_BgCurrentRoom storage.current_room
static s32 levelentry_index;
static bg_portal_data_entry portals[256];
bg_portal_data_entry *g_BgPortals=portals;
#include "bg-special-portals-function.h"
int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    assert(fread(storage.table,1,sizeof storage.table,f)==sizeof storage.table && fgetc(f)==EOF);fclose(f);
    storage.current_room=1;memset(storage.following_data,0xff,sizeof storage.following_data);
    /* Match the four-byte linker gap in the crashing ARM build, and make
     * reading it a sanitizer error rather than an unbounded native loop. */
    assert((uintptr_t)&g_BgCurrentRoom-(uintptr_t)specialportalarray==24);
    __asan_poison_memory_region(storage.linker_padding,sizeof storage.linker_padding);
    for(int stage=0;stage<38;stage++) {
        u8 expected[256]={0};
        for(unsigned at=0;at<sizeof storage.table;) {
            unsigned index=storage.table[at++];
            while(at<sizeof storage.table && storage.table[at]!=0xff) {
                assert(at+1<sizeof storage.table);
                unsigned first=storage.table[at++],last=storage.table[at++];
                if(index==(unsigned)stage)for(unsigned j=first;j<=last;j++)expected[j]|=PORTALFLAG_SPECIAL;
            }
            assert(at<sizeof storage.table && storage.table[at++]==0xff);
        }
        memset(portals,0,sizeof portals);levelentry_index=stage;
        sub_GAME_7F0B37EC();
        for(unsigned j=0;j<256;j++)assert(portals[j].controlbytes1==expected[j]);
    }
    __asan_unpoison_memory_region(storage.linker_padding,sizeof storage.linker_padding);
    puts("PASS actual special-portal loader: all 38 stage indices match retail ranges, Bunker index zero ignores linker padding, no table overread");
}
