#include <ultra64.h>
#include "native_game.h"
#include "native_boot_progress.h"
#include "native_render_reference.h"
#include <string.h>
#include <unistd.h>
#include "token.h"
#include <stdlib.h>
#include <stdio.h>
extern const char *sysGetTokenString(void);
extern char *mkdtemp(char *);
static void require(int condition) {if(!condition)abort();}
static void check(void) {
    tokenSetString(sysGetTokenString());
    if(!tokenFind(1,"-s"))abort();
    const char *stage=tokenFind(1,"-level_");
    if(stage || tokenFind(1,"-hard"))abort();
}
static void check_progress(void) {
    unsigned value=0;
    value=native_boot_total("Checking asset cache",0,1,value);require(value==0);
    value=native_boot_total("Checking ROM",50,100,value);require(value==5);
    value=native_boot_total("Converting models and level",50,100,value);require(value==17);
    value=native_boot_total("Decoding sound effects",50,100,value);require(value==30);
    require(native_boot_total("Decoding audio frames",999999,0,value)==value);
    value=native_boot_total("Decoding music samples",50,100,value);require(value==40);
    value=native_boot_total("Reading object meshes",100,100,value);require(value==55);
    value=native_boot_total("Building object LODs",50,100,value);require(value==62);
    value=native_boot_total("Building room LODs and draw lists",50,100,value);require(value==84);
    require(native_boot_total("Building room LODs and draw lists",10,100,value)==value);
    value=native_boot_total("Saving rendering cache",1,1,value);require(value==98);
    value=native_boot_total("Assets ready",1,1,value);require(value==99);
    require(native_boot_total("Mission ready",1,1,value)==100);
    require(native_boot_total("Building room LODs and draw lists",0xffffffffu,1,0)==98);
    require(native_boot_total("Assets ready",1,1,0)==99); // Warm cache skips conversion.
    puts("PASS weighted overall boot progress: monotonic stages, indeterminate audio, warm cache and ready-only completion");
}
int main(void) {
    check_progress();
    char temporary[]="/tmp/eog-reference-XXXXXX";
    char *directory=mkdtemp(temporary);require(directory!=NULL);
    require(!native_reference_background_requested(NULL));
    require(!native_reference_background_requested(directory));
    require(native_reference_background_enabled(directory));
    char long_path[600];memset(long_path,'x',sizeof long_path-1);long_path[599]=0;
    require(!native_reference_background_requested(long_path));
    char marker[512];snprintf(marker,sizeof marker,"%s/reference-background.flag",directory);
    FILE *file=fopen(marker,"wb");require(file!=NULL);require(fclose(file)==0);
    require(native_reference_background_requested(directory));
    char gpu_marker[512];snprintf(gpu_marker,sizeof gpu_marker,"%s/gpu-background.flag",directory);
    file=fopen(gpu_marker,"wb");require(file!=NULL);require(fclose(file)==0);
    require(native_reference_background_enabled(directory)); /* CPU marker wins. */
    require(unlink(marker)==0);
    require(!native_reference_background_enabled(directory));
    require(unlink(gpu_marker)==0);
    require(native_reference_background_enabled(directory));
    require(rmdir(directory)==0);
    puts("PASS reference background marker: CPU default, GPU opt-in, reference override, null/oversized paths");
    check();
    puts("PASS original token parser: normal title/intro boot with no direct-level override");
    return 0;
}
