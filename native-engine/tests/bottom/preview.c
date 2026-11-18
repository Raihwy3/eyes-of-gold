/* Render actual LCD code with original cartridge fonts, for layout review. */
#define main bottom_tests_main
#include "check.c"
#undef main
static void save(const char *dir,const char *name) {
    native_bottom_draw();char path[1024];snprintf(path,sizeof path,"%s/%s.ppm",dir,name);
    FILE *out=fopen(path,"wb");assert(out);fprintf(out,"P6\n320 240\n255\n");
    for(int y=0;y<240;y++)for(int x=0;x<320;x++) {
        u16 p=buffers[front][x*240+239-y];unsigned char colour[3]={((p>>11)&31)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};
        fwrite(colour,1,3,out);
    }
    fclose(out);
}
int main(int argc,char **argv) {
    assert(argc==3);native_bottom_init();FILE *f=fopen(argv[1],"rb");assert(f);
    for(int font=0;font<2;font++) {
        int k[169],c[94];assert(fread(k,sizeof k,1,f)==1);
        for(int i=0;i<94;i++) {
            int m[4];unsigned char data[1024];assert(fread(m,sizeof m,1,f)==1);assert(fread(data,sizeof data,1,f)==1);
            native_bottom_glyph(font,i,m[0],m[1],m[2],data,32);c[i]=m[3];
        }
        native_bottom_spacing(font,k,c);
    }
    fclose(f);
    struct EogBottomView v={.kind=EOG_BOTTOM_MENU,.context=6,.count=4};strcpy(v.title,"SELECT MODE");
    const char *modes[]={"Select Mission","Multiplayer","Options","Exit"};
    for(int i=0;i<4;i++){strcpy(v.rows[i].label,modes[i]);v.rows[i].tag=i;}v.rows[1].disabled=1;
    native_bottom_publish(&v);save(argv[2],"menu");
    v.kind=EOG_BOTTOM_WATCH;v.context=203;v.page=3;v.count=6;strcpy(v.title,"GAME OPTIONS");
    const char *labels[]={"Music","Sound effects","Look up/down","Auto aim","Aim control","Sight on screen"};
    const char *values[]={"100%","100%","Upright","On","Hold","On"};
    for(int i=0;i<6;i++){strcpy(v.rows[i].label,labels[i]);strcpy(v.rows[i].value,values[i]);v.rows[i].disabled=0;}
    native_bottom_publish(&v);save(argv[2],"watch");
    v.kind=EOG_BOTTOM_GAME;v.context=300;v.count=4;strcpy(v.title,"MISSION OBJECTIVES");
    const char *objectives[]={"Neutralize all alarms","Install covert modem","Intercept data backup","Bungee jump from platform"};
    for(int i=0;i<4;i++){strcpy(v.rows[i].label,objectives[i]);strcpy(v.rows[i].value,i?"Incomplete":"Complete");}
    native_bottom_publish(&v);save(argv[2],"objectives");
    return 0;
}
