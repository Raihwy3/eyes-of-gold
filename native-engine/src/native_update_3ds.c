/* Runs before game workers, audio or Citro3D are initialized. */
#include <3ds.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "native_update.h"
#include "native_update_http.h"
#include "native_version.h"
#include "native_update_ca.h"
static PrintConsole *top_console,*bottom_console;
static struct UpdateJob {
    int download,cancel,done,ok;
    unsigned received,total;
    char metadata[65537],error[160],file[640];
    struct NativeUpdateRelease release;
} job;
static int cancelled(void *unused){(void)unused;return __atomic_load_n(&job.cancel,__ATOMIC_RELAXED);}
static void received(void *unused,size_t done,size_t total){(void)unused;__atomic_store_n(&job.received,(unsigned)done,__ATOMIC_RELAXED);__atomic_store_n(&job.total,(unsigned)total,__ATOMIC_RELAXED);}
static void worker(void *unused) {
    (void)unused;void *socket_buffer=memalign(0x1000,256*1024);
    if(!socket_buffer)goto end;
    if(R_FAILED(socInit(socket_buffer,256*1024))){free(socket_buffer);goto end;}
    struct NativeUpdateTransfer t={0};t.cancel=cancelled;t.progress=received;
    const char *url=NATIVE_UPDATE_API;
    if(job.download){url=job.release.url;t.file=job.file;t.expected=job.release.bytes;t.digest=job.release.digest;}
    else {t.memory=job.metadata;t.capacity=sizeof job.metadata;}
    job.ok=native_update_http(url,&t,native_update_ca,sizeof native_update_ca-1,job.error,sizeof job.error);
    if(job.ok && !job.download)job.ok=native_update_release(job.metadata,t.length,NATIVE_RELEASE_TAG,&job.release);
    socExit();free(socket_buffer);
end:__atomic_store_n(&job.done,1,__ATOMIC_RELEASE);
}
static int start_job(int download) {
    job.download=download;job.cancel=job.done=job.ok=0;job.received=job.total=0;job.error[0]=0;
    return threadCreate(worker,NULL,128*1024,0x3f,0,true)!=NULL;
}
static void draw(const char *stage,const char *message,const char *action,unsigned done,unsigned total) {
    consoleSelect(top_console);consoleClear();
    printf("\x1b[33m\x1b[5;10HE Y E S   O F   G O L D\x1b[0m"
           "\x1b[9;14HLAUNCHER UPDATE"
           "\x1b[13;6H%-38.38s\x1b[16;6HInstalled: " NATIVE_RELEASE_TAG,
           stage);
    if(total){unsigned percent=(unsigned)((uint64_t)done*100/total);if(percent>100)percent=100;char bar[31];for(unsigned i=0;i<30;i++)bar[i]=i<percent*30/100?'=':'-';bar[30]=0;printf("\x1b[20;9H\x1b[33m[%s]\x1b[0m\x1b[23;22H%3u%%",bar,percent);}
    consoleSelect(bottom_console);consoleClear();
    printf("\x1b[33m\x1b[3;3HUPDATE SERVICE\x1b[5;3H----------------------------------\x1b[0m\x1b[8;3H%s",message);
    if((job.download || __atomic_load_n(&job.done,__ATOMIC_ACQUIRE)) && job.release.tag[0])printf("\x1b[13;3HAvailable: %.30s",job.release.tag);
    const char *back=strstr(action,"B:");
    if(!strncmp(action,"A:",2)) {
        size_t length=back?(size_t)(back-action):strlen(action);
        while(length && action[length-1]==' ')length--;
        printf("\x1b[20;3H%.*s",(int)length,action);
    }
    if(back)printf("\x1b[20;23H%s",back);
    printf("\x1b[33m\x1b[25;3H----------------------------------\x1b[0m\x1b[27;3HEyes of Gold " NATIVE_ENGINE_VERSION);
    gfxFlushBuffers();gfxSwapBuffers();
}
static int buttons(void) {
    hidScanInput();u32 keys=hidKeysDown();
    if(keys&KEY_TOUCH){touchPosition p;hidTouchRead(&p);if(p.py>=145 && p.py<=185)keys|=p.px<160?KEY_A:KEY_B;}
    return (keys&KEY_B)?-1:(keys&KEY_A)?1:0;
}
static int prompt(const char *stage,const char *message,const char *action) {
    draw(stage,message,action,0,0);hidScanInput();
    while(aptMainLoop()){int key=buttons();if(key)return key;gspWaitForVBlank();}return 0;
}
static int await_job(int download) {
    u64 started=osGetTime(),paint=0;
    while(aptMainLoop()) {
        if(__atomic_load_n(&job.done,__ATOMIC_ACQUIRE))return job.ok;
        if(buttons()<0 || (!download && osGetTime()-started>=4500)){__atomic_store_n(&job.cancel,1,__ATOMIC_RELAXED);return 0;}
        if(osGetTime()>=paint){draw(download?"DOWNLOADING RELEASE":"CHECKING GITHUB RELEASES",download?"Saving the launcher update...":"Checking for a newer release.","B: Skip / cancel",__atomic_load_n(&job.received,__ATOMIC_RELAXED),__atomic_load_n(&job.total,__ATOMIC_RELAXED));paint=osGetTime()+200;}
        gspWaitForVBlank();
    }
    __atomic_store_n(&job.cancel,1,__ATOMIC_RELAXED);return 0;
}
static int stage_progress(size_t done,size_t total) {
    static u64 paint;
    if(!aptMainLoop() || buttons()<0)return 0;
    if(osGetTime()>=paint){draw("PREPARING UPDATE","Checking the launcher files...","B: Cancel",(unsigned)done,(unsigned)total);paint=osGetTime()+100;}
    return 1;
}
/* Returns zero only when the user chooses to leave without starting the game. */
int native_update_startup(const char *directory,PrintConsole *top,PrintConsole *bottom) {
    top_console=top;bottom_console=bottom;char error[160]={0},flag[640],archive[640];
    if(!native_update_recover(directory,error,sizeof error))return prompt("UPDATE RECOVERY",error,"A: Continue    B: Exit")>0;
    snprintf(flag,sizeof flag,"%s/no-update-check.flag",directory);if(access(flag,F_OK)==0)return 1;
    u32 wifi=0;if(R_FAILED(acInit()))return 1;Result connected=ACU_GetWifiStatus(&wifi);acExit();if(R_FAILED(connected) || !wifi)return 1;
    if(!start_job(0) || !await_job(0))return aptMainLoop();
    int choice=prompt("NEW RELEASE AVAILABLE","Download and install this update?\n\n  ROMs, saves and settings are kept.","A: Install     B: Not now");
    if(choice<=0)return choice<0;
    snprintf(job.file,sizeof job.file,"%s/launcher-update.zip.part",directory);
    if(!start_job(1) || !await_job(1)) {
        if(!__atomic_load_n(&job.done,__ATOMIC_ACQUIRE))return aptMainLoop();
        return prompt("UPDATE NOT INSTALLED","Download failed. The current\n\n  launcher has not been replaced.","A: Play        B: Exit")>0;
    }
    snprintf(archive,sizeof archive,"%s/launcher-update.zip",directory);
    remove(archive);
    if(rename(job.file,archive) || !native_update_install(archive,directory,stage_progress,error,sizeof error))
        return prompt("UPDATE NOT INSTALLED",error[0]?error:"Unable to save update on SD card.","A: Play        B: Exit")>0;
    return prompt("UPDATE INSTALLED","Relaunch to use the new version.\n\n  B continues this running build.","A: Exit        B: Play")<0;
}
