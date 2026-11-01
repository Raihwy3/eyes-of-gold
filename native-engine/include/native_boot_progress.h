#ifndef EOG_NATIVE_BOOT_PROGRESS_H
#define EOG_NATIVE_BOOT_PROGRESS_H
#include <stdint.h>
#include <string.h>
/* Weighted work stages, not a prediction of time remaining. Audio-frame
 * callbacks have no denominator and retain the last measured sample progress. */
static unsigned native_boot_total(const char *stage,unsigned done,unsigned total,unsigned previous) {
    unsigned base=previous,span=0;
    if(!strcmp(stage,"Checking asset cache"))base=0;
    else if(!strcmp(stage,"Checking ROM")){base=0;span=10;}
    else if(!strcmp(stage,"Converting models and level")){base=10;span=15;}
    else if(!strcmp(stage,"Decoding sound effects")){base=25;span=10;}
    else if(!strcmp(stage,"Decoding music samples")){base=35;span=10;}
    else if(!strcmp(stage,"Saving decoded audio") || !strcmp(stage,"Preparing rendering data"))base=45;
    else if(!strcmp(stage,"Reading object meshes")){base=45;span=10;}
    else if(!strcmp(stage,"Building object LODs")){base=55;span=15;}
    else if(!strcmp(stage,"Building room LODs and draw lists")){base=70;span=28;}
    else if(!strcmp(stage,"Saving rendering cache"))base=98;
    else if(!strcmp(stage,"Assets ready"))base=99;
    else if(!strcmp(stage,"Mission ready"))base=100;
    if(total)base+=(unsigned)((uint64_t)(done>total?total:done)*span/total);
    if(base<previous)base=previous;
    return base>100?100:base;
}
#endif
