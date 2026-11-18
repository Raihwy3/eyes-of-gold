#ifndef EOG_NATIVE_BOTTOM_H
#define EOG_NATIVE_BOTTOM_H
#ifdef __cplusplus
extern "C" {
#endif
#define EOG_BOTTOM_ROWS 96
enum {EOG_BOTTOM_MENU=1,EOG_BOTTOM_OPTIONS,EOG_BOTTOM_WATCH,EOG_BOTTOM_GAME};
enum {EOG_BOTTOM_SELECT=1,EOG_BOTTOM_ACTIVATE,EOG_BOTTOM_CHANGE,EOG_BOTTOM_BACK,EOG_BOTTOM_PAGE,EOG_BOTTOM_POINT,EOG_BOTTOM_SCROLL,EOG_BOTTOM_ADJUST};
struct EogBottomRow {char label[192],value[64];int tag,disabled,status;};
#define EOG_OBJECTIVE_TEXTS 24
struct EogObjectiveText {int x,y,width,height,lineheight,overlap,spacing;unsigned colour;char text[512];};
struct EogObjectiveLayout {int count;struct EogObjectiveText text[EOG_OBJECTIVE_TEXTS];};
struct EogBottomView {
    int context,kind,page,selected,count,theme,health,armour;
    char title[64],note[256];
    struct EogBottomRow rows[EOG_BOTTOM_ROWS];
    struct EogObjectiveLayout objectives;
};
struct EogBottomCommand {int context,kind,index,direction;};
/* No original-engine pointers cross into the LCD/input thread. */
void native_bottom_publish(const struct EogBottomView *) __attribute__((weak));
int native_bottom_take(int context,struct EogBottomCommand *) __attribute__((weak));
void native_bottom_glyph(int font,int index,int width,int height,int baseline,const unsigned char *data,int stride) __attribute__((weak));
void native_bottom_spacing(int font,const int *kerning,const int *classes) __attribute__((weak));
int native_bottom_gpu_acquire(void) __attribute__((weak));
void native_bottom_gpu_release(void) __attribute__((weak));
void native_bottom_watch_regions(const int bounds[5][4]) __attribute__((weak));
void native_bottom_seed_open(int value) __attribute__((weak));
int native_bottom_seed_active(void) __attribute__((weak));
unsigned native_bottom_seed_entropy(void) __attribute__((weak));
void native_bottom_init(void);
void native_bottom_draw(void);
void native_bottom_stop(void);
unsigned native_bottom_input(unsigned held,int touching,int tx,int ty,int gameplay,int *dx,int *dy,unsigned *buttons,unsigned *actions) __attribute__((weak));
#ifdef __cplusplus
}
#endif
#endif
