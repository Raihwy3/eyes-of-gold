#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "../../src/bottom_3ds.c"
#include "native_result_ui.h"
static u16 buffers[2][320*240];
static int back,front,swaps,format;
static uint64_t clock_ms;
uint64_t osGetTime(void){return clock_ms;}
void gfxSetScreenFormat(int screen,int f){assert(screen==GFX_BOTTOM);format=f;}
void gfxSetDoubleBuffering(int screen,bool enabled){assert(screen==GFX_BOTTOM && enabled);}
u8 *gfxGetFramebuffer(int screen,int side,u16 *w,u16 *h){assert(screen==GFX_BOTTOM && side==GFX_LEFT);*w=240;*h=320;return (u8 *)buffers[back];}
void GSPGPU_FlushDataCache(void *p,u32 n){assert(p==buffers[back] && n==sizeof buffers[back]);}
void gfxScreenSwapBuffers(int screen,bool stereo){assert(screen==GFX_BOTTOM && !stereo);front=back;back^=1;swaps++;}
static unsigned input(unsigned held,int touch,int x,int y,int game,int *dx,int *dy,unsigned *button,unsigned *action) {
    *dx=*dy=0;*button=*action=0;
    return native_bottom_input(held,touch,x,y,game,dx,dy,button,action);
}
static void tap(int x,int y) {
    int dx,dy;unsigned b,a;input(0,1,x,y,0,&dx,&dy,&b,&a);input(0,0,x,y,0,&dx,&dy,&b,&a);
}
int main(void) {
    assert(!strcmp(native_result_title(0,0,0,1),"MISSION COMPLETE"));
    assert(!strcmp(native_result_title(0,0,0,0),"MISSION FAILED"));
    assert(!strcmp(native_result_title(0,0,1,1),"MISSION ABORTED"));
    assert(!strcmp(native_result_title(0,1,0,1),"KILLED IN ACTION"));
    assert(!strcmp(native_result_title(1,0,0,1),"MISSION BRIEFING"));
    native_bottom_init();
    unsigned char glyph[32*4];memset(glyph,255,sizeof glyph);
    for(int f=0;f<2;f++)for(int i=0;i<94;i++)native_bottom_glyph(f,i,3,4,0,glyph,32);
    struct EogBottomView v={.context=6,.kind=EOG_BOTTOM_MENU,.count=10};strcpy(v.title,"Mode");
    for(int i=0;i<10;i++){snprintf(v.rows[i].label,sizeof v.rows[i].label,"Item %d",i);v.rows[i].tag=i;}
    v.rows[2].disabled=1;
    native_bottom_publish(&v);native_bottom_draw();assert(swaps==1 && format==GSP_RGB565_OES);
    assert(buffers[front][30*240+239-43]==rgb(166,148,77));
    native_bottom_draw();assert(swaps==1); // Stable UI does not spend CPU redrawing.
    int dx,dy;unsigned b,a;
    assert(!(input(EOG_DDOWN|EOG_A,0,0,0,0,&dx,&dy,&b,&a)&(EOG_DDOWN|EOG_A)));
    struct EogBottomCommand c;assert(native_bottom_take(6,&c) && c.kind==EOG_BOTTOM_SELECT && c.index==1);
    assert(native_bottom_take(6,&c) && c.kind==EOG_BOTTOM_ACTIVATE && c.index==1);
    input(0,0,0,0,0,&dx,&dy,&b,&a);native_bottom_draw();assert(swaps==2);
    tap(40,52+2*25+4);assert(!native_bottom_take(6,&c)); // Disabled row never activates.
    tap(40,56);assert(native_bottom_take(6,&c) && c.index==0);
    tap(100,215);assert(native_bottom_take(6,&c) && c.kind==EOG_BOTTOM_ACTIVATE);
    tap(240,215);assert(native_bottom_take(6,&c) && c.kind==EOG_BOTTOM_BACK);
    // Circle Pad selects the original top cursor, not a stale bottom row.
    input(0,0,0,0,0,&dx,&dy,&b,&a);
    native_bottom_menu_stick(80,0);native_bottom_menu_stick(0,0);
    assert(input(EOG_A,0,0,0,0,&dx,&dy,&b,&a)&EOG_A);
    assert(!native_bottom_take(6,&c));
    input(0,0,0,0,0,&dx,&dy,&b,&a);
    // D-pad takes ownership back, including directions that do not move rows.
    input(EOG_DLEFT,0,0,0,0,&dx,&dy,&b,&a);
    assert(!(input(EOG_A,0,0,0,0,&dx,&dy,&b,&a)&EOG_A));
    assert(native_bottom_take(6,&c) && c.kind==EOG_BOTTOM_ACTIVATE);
    // Switching owners while A is held cannot generate a second selection.
    native_bottom_menu_stick(80,0);
    assert(!(input(EOG_A,0,0,0,0,&dx,&dy,&b,&a)&EOG_A));
    assert(!native_bottom_take(6,&c));
    input(0,0,0,0,0,&dx,&dy,&b,&a);
    native_bottom_menu_stick(2,-3); // Idle drift never takes ownership.
    tap(40,56);assert(native_bottom_take(6,&c) && c.kind==EOG_BOTTOM_ACTIVATE);
    assert(!(input(EOG_A,0,0,0,0,&dx,&dy,&b,&a)&EOG_A));
    assert(native_bottom_take(6,&c) && c.kind==EOG_BOTTOM_ACTIVATE && c.index==0);
    input(0,0,0,0,0,&dx,&dy,&b,&a);
    v.context=106;v.kind=EOG_BOTTOM_OPTIONS;native_bottom_publish(&v);
    assert(input(EOG_DDOWN,0,0,0,0,&dx,&dy,&b,&a)==EOG_DDOWN); // Existing native panel owns D-pad.
    tap(250,56);assert(native_bottom_take(106,&c) && c.kind==EOG_BOTTOM_CHANGE && c.direction==1);
    v.context=300;v.kind=EOG_BOTTOM_GAME;native_bottom_publish(&v);
    input(0,1,90,100,1,&dx,&dy,&b,&a);assert(!dx && !dy);
    input(0,1,100,92,1,&dx,&dy,&b,&a);assert(dx==30 && dy==24 && !b && !a);
    input(0,0,100,92,1,&dx,&dy,&b,&a);
    // No added button bars: lower screen remains a look pad. Hold opens watch once.
    input(0,1,100,220,1,&dx,&dy,&b,&a);assert(!a && !b);
    clock_ms+=649;input(0,1,100,220,1,&dx,&dy,&b,&a);assert(!b);
    clock_ms++;input(0,1,100,220,1,&dx,&dy,&b,&a);assert(b==0x1000 && !a);
    clock_ms+=1000;input(0,1,100,220,1,&dx,&dy,&b,&a);assert(!b);
    v.context=200;v.kind=EOG_BOTTOM_WATCH;v.page=0;native_bottom_publish(&v);
    input(0,1,100,100,0,&dx,&dy,&b,&a);assert(!native_bottom_take(200,&c)); // No hidden CPU controls before GPU ownership.
    assert(native_bottom_gpu_acquire());
    int first_bounds[5][4]={{10,183,50,204},{60,183,100,204},{110,183,150,204},{160,183,200,204},{210,183,250,204}};
    native_bottom_watch_regions(first_bounds);
    input(0,1,100,190,0,&dx,&dy,&b,&a);input(0,0,100,190,0,&dx,&dy,&b,&a);
    assert(!native_bottom_take(200,&c)); // Opening hold cannot select a page.
    tap(100,190);assert(native_bottom_take(200,&c) && c.kind==EOG_BOTTOM_PAGE && c.index==1);
    tap(70,10);assert(native_bottom_take(200,&c) && c.kind==EOG_BOTTOM_POINT); // Whole LCD maps to original canvas.
    input(0,1,100,100,0,&dx,&dy,&b,&a);clock_ms+=650;
    input(0,1,100,100,0,&dx,&dy,&b,&a);assert(native_bottom_take(200,&c) && c.kind==EOG_BOTTOM_BACK);
    input(0,0,100,100,0,&dx,&dy,&b,&a);assert(!native_bottom_take(200,&c));
    input(0,1,200,100,0,&dx,&dy,&b,&a);input(0,1,150,100,0,&dx,&dy,&b,&a);
    assert(native_bottom_take(200,&c) && c.kind==EOG_BOTTOM_PAGE && c.index==1);
    input(0,0,150,100,0,&dx,&dy,&b,&a);assert(!native_bottom_take(200,&c));
    // GPU-owned watch output suppresses CPU LCD swaps and uses the exact
    // inverse of the GPU crop, plus actual projected page-strip hit regions.
    assert(native_bottom_gpu_acquire());int before_swaps=swaps;
    native_bottom_draw();assert(swaps==before_swaps);
    int bounds[5][4]={{10,210,50,225},{60,210,100,225},{110,210,150,225},{160,210,200,225},{210,210,250,225}};
    native_bottom_watch_regions(bounds);
    tap(70,215);assert(native_bottom_take(200,&c) && c.kind==EOG_BOTTOM_PAGE && c.index==1);
    tap(100,100);assert(native_bottom_take(200,&c) && c.kind==EOG_BOTTOM_POINT && c.index==native_watch_page_touch_x(100,0) && c.direction==native_watch_page_touch_y(100,0));
    input(0,1,100,100,0,&dx,&dy,&b,&a);input(0,1,100,80,0,&dx,&dy,&b,&a);
    assert(native_bottom_take(200,&c) && c.kind==EOG_BOTTOM_SCROLL && c.direction==1);
    input(0,0,100,80,0,&dx,&dy,&b,&a);assert(!native_bottom_take(200,&c));
    native_bottom_gpu_release();v.kind=EOG_BOTTOM_GAME;v.context=300;native_bottom_publish(&v);native_bottom_draw();assert(swaps==before_swaps+1);
    v.kind=EOG_BOTTOM_WATCH;v.context=200;native_bottom_publish(&v);assert(native_bottom_gpu_acquire());
    // Old-context events are discarded on transition; bounded queue wraps safely.
    for(int i=0;i<40;i++)tap(40,80);
    assert(!native_bottom_take(300,&c));
    native_bottom_gpu_release();v.kind=EOG_BOTTOM_GAME;v.context=301;v.theme=0;v.objectives.count=2;
    struct EogObjectiveText *label=&v.objectives.text[0],*status=&v.objectives.text[1];
    *label=(struct EogObjectiveText){.x=60,.y=50,.width=100,.height=50,.lineheight=10,.overlap=5,.colour=0x00ff00b0};strcpy(label->text,"a: Test\n");
    *status=*label;status->x=175;status->colour=0xa0ffa0f0;strcpy(status->text,"Complete\n");
    unsigned char blank[128]={0};native_bottom_glyph(0,'a'-33,3,4,0,blank,32);
    native_bottom_publish(&v);native_bottom_draw();
    int label_x=(int)__builtin_lroundf(native_watch_content_x(61)),label_y=(int)__builtin_lroundf(native_watch_content_y(50));
    assert(buffers[front][label_x*240+239-label_y]==rgb(0,(255*176+24*79)/255,0));
    assert(buffers[front][16*240+239-17]==rgb(0,24,0)); // No extra gameplay title/layout.
    int status_x=(int)__builtin_lroundf(native_watch_content_x(176));
    assert(buffers[front][status_x*240+239-label_y]!=buffers[front][label_x*240+239-label_y]);
    // Partial glyph coverage must retain the original alpha, not a boost.
    memset(glyph,128,sizeof glyph);native_bottom_glyph(1,'a'-33,3,4,0,glyph,32);
    native_bottom_draw();unsigned expected_alpha=128*176/255;
    assert(buffers[front][label_x*240+239-label_y]==rgb(0,(255*expected_alpha+24*(255-expected_alpha))/255,0));
    const unsigned char pattern[4]={0,255,255,0};
    for(int y=0;y<2;y++)for(int x=0;x<2;x++)assert(native_glyph_sample(pattern,2,2,2,x,y,2,2)==pattern[y*2+x]);
    assert(native_glyph_sample(pattern,2,2,2,1,1,3,3)==128);
    assert(native_glyph_sample(pattern,2,2,2,0,0,3,3)==0);
    assert(native_glyph_sample(pattern,2,2,2,2,0,3,3)==255);
    native_bottom_stop();assert(native_bottom_input(EOG_A,1,0,0,0,&dx,&dy,&b,&a)==EOG_A);
    puts("PASS actual bottom LCD: rotated RGB565 buffers, stable-frame reuse, D-pad/touch rows, disabled choices, option changes, full-screen drag-look, timed hold/open/back, original bottom page strips, horizontal swipes, transition gating, GPU ownership, inverse touch mapping, result headings, original watch objective glyph layout/colours and bounded queue");
    return 0;
}
