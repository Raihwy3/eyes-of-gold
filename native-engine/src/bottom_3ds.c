#include "native_glyph_sample.h"
/* Independent bottom LCD UI. The GPU and top LCD stay owned by Fast3D. */
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "native_bottom.h"
#include "native_menu_focus.h"
#include "native_pad.h"
#include "native_theme.h"
#include "native_watch_mirror.h"
#ifdef __3DS__
#include "native_derived.h"
#else
#include "native_bottom_background.h"
#endif
struct Glyph {unsigned char pixels[1024];int w,h,baseline;};
static struct Glyph fonts[2][94];
static int kerns[2][169],classes[2][94];
static int gpu_owned;
static int page_bounds[5][4];
static uint64_t touch_started;
static int touch_start_x;
static LightLock lock;
static struct EogBottomView view,draw_view;
static struct EogBottomCommand queue[16];
static unsigned read_pos,write_pos,last_buttons;
static unsigned generation,last_drawn;
static int menu_stick_owned,menu_accept_bottom;
static int active,stopped,focus,scroll,last_touch,touch_x,touch_y,touch_start_y,scroll_touch_y,moved,nav_dir,nav_ticks,release_gate;
static u16 pixels[320*240];
static int min(int a,int b){return a<b?a:b;}
static int max(int a,int b){return a>b?a:b;}
static void reveal(void);
void native_bottom_init(void){LightLock_Init(&lock);active=1;stopped=0;menu_stick_owned=menu_accept_bottom=0;}
void native_bottom_menu_stick(int x,int y) {
    LightLock_Lock(&lock);
    if(view.kind==EOG_BOTTOM_MENU && (x>20 || x< -20 || y>20 || y< -20))menu_stick_owned=1;
    LightLock_Unlock(&lock);
}
int native_bottom_gpu_acquire(void) {
    LightLock_Lock(&lock);
    int okay=!__atomic_load_n(&stopped,__ATOMIC_ACQUIRE);
    if(okay){gpu_owned=1;memset(page_bounds,0,sizeof page_bounds);gfxSetScreenFormat(GFX_BOTTOM,GSP_RGB565_OES);gfxSetDoubleBuffering(GFX_BOTTOM,true);}
    LightLock_Unlock(&lock);return okay;
}
void native_bottom_gpu_release(void){LightLock_Lock(&lock);gpu_owned=0;generation++;LightLock_Unlock(&lock);}
void native_bottom_watch_regions(const int bounds[5][4]){LightLock_Lock(&lock);memcpy(page_bounds,bounds,sizeof page_bounds);LightLock_Unlock(&lock);}
void native_bottom_stop(void){__atomic_store_n(&stopped,1,__ATOMIC_RELEASE);}
static int bottom_stopped(void){return __atomic_load_n(&stopped,__ATOMIC_ACQUIRE);}
void native_bottom_publish(const struct EogBottomView *v) {
    if(!active || bottom_stopped() || v->count<0 || v->count>EOG_BOTTOM_ROWS)return;
    LightLock_Lock(&lock);
    int selection_changed=view.context!=v->context || view.selected!=v->selected;
    if(view.context!=v->context){read_pos=write_pos;focus=v->selected;scroll=0;nav_dir=nav_ticks=0;release_gate=last_touch;}
    else if(view.selected!=v->selected)focus=v->selected;
    if(memcmp(&view,v,sizeof view))generation++;
    view=*v;focus=max(0,min(focus,view.count-1));
    if(selection_changed)reveal();
    LightLock_Unlock(&lock);
}
int native_bottom_take(int context,struct EogBottomCommand *out) {
    if(!active)return 0;
    LightLock_Lock(&lock);
    while(read_pos!=write_pos) {
        *out=queue[read_pos++%16];
        if(out->context==context){LightLock_Unlock(&lock);return 1;}
    }
    LightLock_Unlock(&lock);return 0;
}
void native_bottom_glyph(int font,int index,int w,int h,int baseline,const unsigned char *data,int stride) {
    if(!active || font<0 || font>1 || index<0 || index>=94 || w<0 || w>32 || h<0 || h>32 || baseline< -32 || baseline>32 || !data || stride<w)return;
    struct Glyph glyph={.w=w,.h=h,.baseline=baseline};
    for(int y=0;y<h;y++)memcpy(glyph.pixels+y*32,data+y*stride,w);
    LightLock_Lock(&lock);fonts[font][index]=glyph;
    generation++;
    LightLock_Unlock(&lock);
}
static void command(int kind,int index,int direction) {
    if(write_pos-read_pos==16)read_pos++;
    queue[write_pos++%16]=(struct EogBottomCommand){view.context,kind,index,direction};
}
void native_bottom_spacing(int font,const int *kerning,const int *indices) {
    if(font<0 || font>1 || !kerning || !indices)return;
    int k[169],c[94];
    for(int i=0;i<169;i++){if(kerning[i]< -32 || kerning[i]>32)return;k[i]=kerning[i];}
    for(int i=0;i<94;i++){if(indices[i]<0 || indices[i]>12)return;c[i]=indices[i];}
    LightLock_Lock(&lock);memcpy(kerns[font],k,sizeof k);memcpy(classes[font],c,sizeof c);generation++;LightLock_Unlock(&lock);
}
static int row_height(void){return view.kind==EOG_BOTTOM_GAME?35:25;}
static int visible_rows(void){return view.kind==EOG_BOTTOM_GAME?4:6;}
static void reveal(void){int visible=visible_rows();scroll=min(scroll,max(0,view.count-visible));if(focus<scroll)scroll=focus;if(focus>=scroll+visible)scroll=focus-visible+1;}
unsigned native_bottom_input(unsigned held,int touching,int tx,int ty,int gameplay,int *dx,int *dy,unsigned *buttons,unsigned *actions) {
    (void)actions;
    if(!active || bottom_stopped())return held;
    LightLock_Lock(&lock);
    int previous_focus=focus,previous_scroll=scroll;
    if(release_gate){if(!touching)release_gate=0;touching=last_touch=0;}
    unsigned down=held&~last_buttons;last_buttons=held;
    if(view.kind==EOG_BOTTOM_WATCH && !gpu_owned){last_touch=0;LightLock_Unlock(&lock);return held;}
    if(touching && !last_touch){touch_started=osGetTime();touch_start_x=tx;touch_start_y=scroll_touch_y=ty;moved=0;}
    int total_x=tx-touch_start_x,total_y=ty-touch_start_y;
    if(view.kind==EOG_BOTTOM_GAME) {
        if(gameplay && touching && last_touch) {
            if(total_x>8 || total_x< -8 || total_y>8 || total_y< -8)moved=1;
            if(moved!=2){*dx=max(-80,min(80,(tx-touch_x)*3));*dy=max(-80,min(80,(touch_y-ty)*3));}
            if(!moved && osGetTime()-touch_started>=650){*buttons|=0x1000;moved=2;}
        }
    } else if(view.kind) {
        if(view.kind==EOG_BOTTOM_MENU) {
            if(touching || last_touch || (held&(EOG_DUP|EOG_DDOWN|EOG_DLEFT|EOG_DRIGHT)))menu_stick_owned=0;
            int dir=held&EOG_DUP?-1:held&EOG_DDOWN?1:0,step=0;
            if(dir!=nav_dir){nav_dir=dir;nav_ticks=0;step=dir;}
            else if(dir && ++nav_ticks>15 && nav_ticks%6==0)step=dir;
            if(step && view.count){focus=(focus+step+view.count)%view.count;reveal();command(EOG_BOTTOM_SELECT,focus,0);}
            if(down&EOG_A)menu_accept_bottom=!menu_stick_owned;
            if(menu_accept_bottom && (down&EOG_A))command(EOG_BOTTOM_ACTIVATE,focus,0);
            if(down&EOG_B)command(EOG_BOTTOM_BACK,0,0);
            held&=~(EOG_DUP|EOG_DDOWN|EOG_DLEFT|EOG_DRIGHT|EOG_B|(menu_accept_bottom?EOG_A:0));
        }
        if(touching && last_touch && moved!=2) {
            if(view.kind==EOG_BOTTOM_WATCH && (total_x>35 || total_x< -35) && total_x*total_x>total_y*total_y) {
                command(EOG_BOTTOM_PAGE,(view.page+(total_x<0?1:4))%5,0);moved=2;
            } else if(total_y>12 || total_y< -12) {
                moved=1;
                if(ty-scroll_touch_y>15){if(gpu_owned && view.kind==EOG_BOTTOM_WATCH)command(EOG_BOTTOM_SCROLL,0,-1);else scroll=max(0,scroll-1);scroll_touch_y=ty;}
                if(ty-scroll_touch_y< -15){if(gpu_owned && view.kind==EOG_BOTTOM_WATCH)command(EOG_BOTTOM_SCROLL,0,1);else scroll=min(max(0,view.count-visible_rows()),scroll+1);scroll_touch_y=ty;}
            } else if(!moved && total_x<=8 && total_x>=-8 && total_y<=8 && total_y>=-8 && osGetTime()-touch_started>=650) {
                command(EOG_BOTTOM_BACK,0,gpu_owned?1:0);moved=2;
            }
        }
        if(!touching && last_touch && !moved) {
            if(view.kind==EOG_BOTTOM_WATCH && gpu_owned) {
                int page=-1;
                for(int i=0;i<5;i++)if(page_bounds[i][2]>page_bounds[i][0] && page_bounds[i][3]>page_bounds[i][1] && touch_x>=page_bounds[i][0]-4 && touch_x<=page_bounds[i][2]+4 && touch_y>=page_bounds[i][1]-4 && touch_y<=page_bounds[i][3]+4)page=i;
                if(page>=0)command(EOG_BOTTOM_PAGE,page,0);
                else command(EOG_BOTTOM_POINT,native_watch_page_touch_x(touch_x,view.page),native_watch_page_touch_y(touch_y,view.page));
            } else if(touch_y>=205 && touch_y<228 && touch_x>=12 && touch_x<308)command(touch_x<160?EOG_BOTTOM_ACTIVATE:EOG_BOTTOM_BACK,focus,0);
            else if(touch_y>=52 && touch_y<202 && touch_x>=24 && touch_x<296) {
                int row=scroll+(touch_y-52)/row_height();
                if(row<view.count && !view.rows[row].disabled){focus=row;command(view.kind==EOG_BOTTOM_OPTIONS?EOG_BOTTOM_CHANGE:EOG_BOTTOM_ACTIVATE,row,touch_x<190?-1:1);}
            }
        }
    }
    if(scroll!=previous_scroll && view.kind!=EOG_BOTTOM_GAME && (focus<scroll || focus>=scroll+visible_rows())){focus=scroll;command(EOG_BOTTOM_SELECT,focus,0);}
    last_touch=touching;touch_x=tx;touch_y=ty;
    if(focus!=previous_focus || scroll!=previous_scroll)generation++;
    LightLock_Unlock(&lock);return held;
}
static u16 rgb(unsigned r,unsigned g,unsigned b){return (u16)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));}
static void rect(int x,int y,int w,int h,u16 c){for(int j=max(0,y);j<min(240,y+h);j++)for(int i=max(0,x);i<min(320,x+w);i++)pixels[j*320+i]=c;}
static unsigned menu_character(unsigned c){return (draw_view.kind==EOG_BOTTOM_MENU || draw_view.kind==EOG_BOTTOM_OPTIONS) && c>='a' && c<='z'?c-32:c;}
static void text(int font,int x,int y,const char *s,u16 colour,int right,int lines) {
    int origin=x,start_y=y,previous=0;
    for(;*s && y<start_y+lines*12;s++) {
        unsigned ch=menu_character((unsigned char)*s);
        if(ch=='\n'){x=origin;y+=12;previous=0;continue;}
        if(ch==' '){x+=4;previous=0;continue;}
        if(ch<33 || ch>126)continue;
        struct Glyph *g=&fonts[font][ch-33];
        if(!g->w)g=&fonts[1-font][ch-33];
        int current=classes[font][ch-33];
        x-=kerns[font][previous*13+current];previous=current;
        if(x+g->w>right){x=origin;y+=12;if(y>=start_y+lines*12)break;}
        for(int j=0;j<g->h;j++)for(int i=0;i<g->w;i++) {
            int px=x+i,py=y+j+g->baseline;
            unsigned alpha=g->pixels[j*32+i];
            if(px>=0 && px<right && py>=0 && py<240 && alpha>40) {
                u16 old=pixels[py*320+px];
                unsigned r=(((colour>>11)&31)*alpha+((old>>11)&31)*(255-alpha))/255;
                unsigned green=(((colour>>5)&63)*alpha+((old>>5)&63)*(255-alpha))/255;
                unsigned b=((colour&31)*alpha+(old&31)*(255-alpha))/255;
                pixels[py*320+px]=(r<<11)|(green<<5)|b;
            }
        }
        x+=g->w+1;
    }
}
/* Draw the same glyph rectangles/layout recorded from the original watch
 * objective page, using its Bank Gothic font, spacing, clipping and colours. */
static void objective_text(const struct EogObjectiveText *t,int theme) {
    unsigned r=t->colour>>24,g=(t->colour>>16)&255,b=(t->colour>>8)&255,a=t->colour&255;
    native_theme_rgb(theme,&r,&g,&b);
    int x=t->x,y=t->y,previous='H'-33;
    int clipx0=(int)native_watch_content_x(t->x),clipx1=(int)native_watch_content_x(t->x+t->width);
    int clipy0=(int)native_watch_content_y(t->y),clipy1=(int)native_watch_content_y(t->y+t->height);
    for(const unsigned char *p=(const unsigned char *)t->text;*p;p++) {
        if(*p==' '){x+=5;previous='H'-33;continue;}
        if(*p=='\n'){y+=t->overlap>=0 && x==t->x?t->overlap:t->lineheight;x=t->x;previous='H'-33;continue;}
        if(*p<33 || *p>126)continue;
        int c=*p-33;struct Glyph *glyph=&fonts[1][c];
        x+=1-kerns[1][classes[1][previous]*13+classes[1][c]]-t->spacing;previous=c;
        int gx0=(int)__builtin_lroundf(native_watch_content_x(x)),gx1=(int)__builtin_lroundf(native_watch_content_x(x+glyph->w));
        int gy0=(int)__builtin_lroundf(native_watch_content_y(y+glyph->baseline)),gy1=(int)__builtin_lroundf(native_watch_content_y(y+glyph->baseline+glyph->h));
        int w=gx1-gx0,h=gy1-gy0;
        for(int yy=max(max(gy0,clipy0),0);yy<min(min(gy1,clipy1),240) && h>0;yy++)for(int xx=max(max(gx0,clipx0),0);xx<min(min(gx1,clipx1),320) && w>0;xx++) {
            unsigned alpha=native_glyph_sample(glyph->pixels,32,glyph->w,glyph->h,xx-gx0,yy-gy0,w,h)*a/255;u16 old=pixels[yy*320+xx];
            unsigned oldr=((old>>11)&31)*255/31,oldg=((old>>5)&63)*255/63,oldb=(old&31)*255/31;
            pixels[yy*320+xx]=rgb((r*alpha+oldr*(255-alpha))/255,(g*alpha+oldg*(255-alpha))/255,(b*alpha+oldb*(255-alpha))/255);
        }
        x+=glyph->w;
    }
}
static int text_width(int font,const char *s) {
    int width=0,previous=0;
    for(;*s;s++) {
        unsigned ch=menu_character((unsigned char)*s);
        if(ch==' '){width+=4;previous=0;continue;}
        if(ch<33 || ch>126)continue;
        int current=classes[font][ch-33];
        width+=fonts[font][ch-33].w+1-kerns[font][previous*13+current];previous=current;
    }
    return width;
}
static void centred(int y,const char *s,u16 colour){text(0,max(20,160-text_width(0,s)/2),y,s,colour,300,1);}
void native_bottom_draw(void) {
    if(!active || bottom_stopped())return;
    if(LightLock_TryLock(&lock))return;
    if(gpu_owned || view.kind==EOG_BOTTOM_WATCH){LightLock_Unlock(&lock);return;}
    if(last_drawn==generation){LightLock_Unlock(&lock);return;}
    last_drawn=generation;draw_view=view;int first=scroll,selected=focus;
    if(!draw_view.kind){LightLock_Unlock(&lock);return;}
    unsigned r=0,g=220,b=0;native_theme_rgb(draw_view.theme,&r,&g,&b);
    int game=draw_view.kind==EOG_BOTTOM_GAME;
    u16 accent=game?rgb(r,g,b):rgb(235,214,122),ink=game?rgb(209,217,166):rgb(229,214,163);
    int watch_objectives=game && draw_view.objectives.count>0;
    if(watch_objectives) {
        unsigned br=0,bg=24,bb=0;native_theme_rgb(draw_view.theme,&br,&bg,&bb);rect(0,0,320,240,rgb(br,bg,bb));
        for(int i=0;i<draw_view.objectives.count && i<EOG_OBJECTIVE_TEXTS;i++)objective_text(&draw_view.objectives.text[i],draw_view.theme);
    }
    else if(game){rect(0,0,320,240,rgb(3,11,4));text(0,16,17,draw_view.title,accent,304,1);}
    else {
        for(int i=0;i<320*240;i++){unsigned grey=original_bottom_background[i];pixels[i]=rgb(grey,grey,grey);}
        rect(12,12,296,216,rgb(4,4,3));
        centred(23,draw_view.title,accent);rect(30,43,260,1,rgb(166,148,77));rect(30,198,260,1,rgb(166,148,77));
        centred(211,"A: select   B: back",rgb(204,189,140));
    }
    int height=game?35:25,visible=game?4:6,top=game?65:52;
    for(int i=0;i<visible && first+i<draw_view.count && !watch_objectives;i++) {
        struct EogBottomRow *row=&draw_view.rows[first+i];int y=top+i*height;
        int chosen=first+i==selected && row->tag>=0 && !game;
        if(chosen)rect(28,y-4,264,height-1,rgb(48,40,14));
        u16 colour=row->disabled?rgb(125,119,95):chosen?accent:ink;
        text(0,game?16:34,y,row->label,colour,row->value[0] && !game?190:298,1);
        if(row->value[0])text(0,game?24:198,y+(game?14:0),row->value,row->status==2?rgb(255,90,70):accent,298,1);
    }
    if(!draw_view.count && !watch_objectives)text(0,24,game?65:58,draw_view.note,ink,298,8);
    if(!game && draw_view.count>visible) {
        int track=140,y=52;
        rect(301,y,1,track,rgb(50,60,40));
        rect(300,y+first*track/draw_view.count,3,max(4,visible*track/draw_view.count),accent);
    }
    static int prepared;
    if(!prepared){gfxSetScreenFormat(GFX_BOTTOM,GSP_RGB565_OES);gfxSetDoubleBuffering(GFX_BOTTOM,true);prepared=1;}
    u16 w,h;u16 *lcd=(u16 *)gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,&w,&h);
    for(int y=0;y<240;y++)for(int x=0;x<320;x++)lcd[x*240+239-y]=pixels[y*320+x];
    GSPGPU_FlushDataCache(lcd,320*240*2);gfxScreenSwapBuffers(GFX_BOTTOM,false);
    LightLock_Unlock(&lock);
}
