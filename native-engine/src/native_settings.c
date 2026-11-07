/* Small native preferences, independent of the N64 EEPROM bit layout.
 * Sensitivities retain legacy gain units in the file/API so existing settings
 * keep their speed. The UI displays gain*2/5: 250 is the new 100% baseline. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "native_settings.h"
#include "native_console.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
static int style=-1,crosshair,watch_colour,aspect=-1,aa=1,fps_limit=60,sensitivity=250,sharpness=0,vignette=0,bloom=0,grain=0,skin=0,glass=1,lod=1,view_distance=2;
static int watch_clock;
int native_setting_watch_clock(void){return __atomic_load_n(&watch_clock,__ATOMIC_RELAXED);}
int native_watch_clock_time(int fallback){
    if(!native_setting_watch_clock())return fallback;
    struct timeval now;struct tm local;
    if(gettimeofday(&now,NULL)!=0 || !localtime_r(&now.tv_sec,&local))return fallback;
    return ((local.tm_hour%12)*3600+local.tm_min*60+local.tm_sec)*60+(int)(now.tv_usec*60/1000000);
}
static int crosshair_shape,crosshair_opacity=-1;
int native_setting_crosshair_opacity(void){return __atomic_load_n(&crosshair_opacity,__ATOMIC_RELAXED);}
int native_setting_crosshair_shape(void){return __atomic_load_n(&crosshair_shape,__ATOMIC_RELAXED);}
static int flick_stick,aim_movement,gyro_filter,always_crosshair,vertical_sensitivity=250;
int native_setting_gyro_filter(void){return __atomic_load_n(&gyro_filter,__ATOMIC_RELAXED);}
int native_setting_always_crosshair(void){return __atomic_load_n(&always_crosshair,__ATOMIC_RELAXED);}
int native_setting_vertical_sensitivity(void){return __atomic_load_n(&vertical_sensitivity,__ATOMIC_RELAXED);}
int native_setting_aim_movement(void){return __atomic_load_n(&aim_movement,__ATOMIC_RELAXED);}
static int gyro_space,gyro_axis,aim_lock,stamina_sway,steady_scope=1;
int native_setting_gyro_space(void){return __atomic_load_n(&gyro_space,__ATOMIC_RELAXED);}
int native_setting_gyro_axis(void){return __atomic_load_n(&gyro_axis,__ATOMIC_RELAXED);}
int native_setting_aim_lock(void){return __atomic_load_n(&aim_lock,__ATOMIC_RELAXED);}
int native_setting_stamina_sway(void){return __atomic_load_n(&stamina_sway,__ATOMIC_RELAXED);}
int native_setting_steady_scope(void){return __atomic_load_n(&steady_scope,__ATOMIC_RELAXED);}

int native_setting_flick(void){return __atomic_load_n(&flick_stick,__ATOMIC_RELAXED);}
static int stereo_auto=1;
int native_setting_stereo_auto(void){return __atomic_load_n(&stereo_auto,__ATOMIC_RELAXED);}
static int stereo_convergence=100;
int native_setting_stereo_convergence(void){return __atomic_load_n(&stereo_convergence,__ATOMIC_RELAXED);}
static int stereo_comfort=0;
int native_setting_stereo_comfort(void){return __atomic_load_n(&stereo_comfort,__ATOMIC_RELAXED);}
static int stereo_hud_depth=75;
int native_setting_stereo_hud_depth(void){return __atomic_load_n(&stereo_hud_depth,__ATOMIC_RELAXED);}
static int stereo_crosshair_hud=0;
int native_setting_stereo_crosshair_hud(void){return __atomic_load_n(&stereo_crosshair_hud,__ATOMIC_RELAXED);}
static int stereo_ghost_contrast=100;
int native_setting_stereo_ghost_contrast(void){return __atomic_load_n(&stereo_ghost_contrast,__ATOMIC_RELAXED);}
static int stereo_black_floor=0;
int native_setting_stereo_black_floor(void){return __atomic_load_n(&stereo_black_floor,__ATOMIC_RELAXED);}
static int stereo_strength=100;
int native_setting_stereo_strength(void){return __atomic_load_n(&stereo_strength,__ATOMIC_RELAXED);}
static int gyro_mode,gyro_sensitivity=250,touch_sensitivity=250;
int native_setting_touch_sensitivity(void){return __atomic_load_n(&touch_sensitivity,__ATOMIC_RELAXED);}
int native_setting_gyro(void){return __atomic_load_n(&gyro_mode,__ATOMIC_RELAXED);}
int native_setting_gyro_sensitivity(void){return __atomic_load_n(&gyro_sensitivity,__ATOMIC_RELAXED);}
static int dynamic_lod=1,vignette_mode,audio_mixer=1;
int native_setting_audio_mixer(void){return __atomic_load_n(&audio_mixer,__ATOMIC_RELAXED);}
static int randomiser,randomiser_seed=1;
int native_setting_randomiser(void){return __atomic_load_n(&randomiser,__ATOMIC_RELAXED);}
int native_setting_randomiser_seed(void){return __atomic_load_n(&randomiser_seed,__ATOMIC_RELAXED);}
const char *native_randomiser_label(void){return "Randomiser";}
const char *native_randomiser_seed_label(void){return "Randomise seed";}
const char *native_randomiser_seed_text(void){static char text[8];snprintf(text,sizeof text,"%06d",native_setting_randomiser_seed());return text;}
static int subsistence;
int native_setting_subsistence(void){return __atomic_load_n(&subsistence,__ATOMIC_RELAXED);}
const char *native_subsistence_label(void){return "Subsistence";}
static int unlock_all,guard_visibility,guard_ai_hz=30;
int native_setting_guard_visibility(void){return __atomic_load_n(&guard_visibility,__ATOMIC_RELAXED);}
int native_setting_guard_ai_hz(void){return __atomic_load_n(&guard_ai_hz,__ATOMIC_RELAXED);}
int native_setting_unlock_all(void){return __atomic_load_n(&unlock_all,__ATOMIC_RELAXED);}
static unsigned render_state;
static int game_values[7]={-1,-1,-1,-1,-1,-1,-1};
static int volumes[2]={-1,-1};
static char settings_path[512];
static int config_missing;
int native_setting_lod(void){return __atomic_load_n(&lod,__ATOMIC_RELAXED);}
int native_setting_lod_strength(void){int v=native_setting_lod();return v==4?__atomic_load_n(&dynamic_lod,__ATOMIC_RELAXED):v;}
void native_setting_dynamic_lod(int v){if(v>=1 && v<=3)__atomic_store_n(&dynamic_lod,v,__ATOMIC_RELAXED);}
unsigned native_setting_render_state(void){return __atomic_load_n(&render_state,__ATOMIC_RELAXED);}
void native_setting_set_render_state(unsigned v){__atomic_store_n(&render_state,v,__ATOMIC_RELAXED);}
int native_setting_vignette_mode(void){return __atomic_load_n(&vignette_mode,__ATOMIC_RELAXED);}

int native_setting_view_distance(void){return __atomic_load_n(&view_distance,__ATOMIC_RELAXED);}
int native_setting_glass(void){return __atomic_load_n(&glass,__ATOMIC_RELAXED);}
int native_setting_skin(void){return __atomic_load_n(&skin,__ATOMIC_RELAXED);}
int native_setting_style(void){return __atomic_load_n(&style,__ATOMIC_RELAXED);}
int native_setting_crosshair(void){return __atomic_load_n(&crosshair,__ATOMIC_RELAXED);}
int native_setting_watch(void){return __atomic_load_n(&watch_colour,__ATOMIC_RELAXED);}
int native_setting_aspect(void){return __atomic_load_n(&aspect,__ATOMIC_RELAXED);}
int native_setting_vignette(void){return __atomic_load_n(&vignette,__ATOMIC_RELAXED);}
int native_setting_grain(void){return __atomic_load_n(&grain,__ATOMIC_RELAXED);}
int native_setting_bloom(void){return __atomic_load_n(&bloom,__ATOMIC_RELAXED);}
int native_setting_sharpness(void){return __atomic_load_n(&sharpness,__ATOMIC_RELAXED);}
int native_setting_sensitivity(void){return __atomic_load_n(&sensitivity,__ATOMIC_RELAXED);}
int native_setting_fps(void){return __atomic_load_n(&fps_limit,__ATOMIC_RELAXED);}
int native_setting_aa(void){return __atomic_load_n(&aa,__ATOMIC_RELAXED);}
int native_setting_game(int index){return index==7?native_setting_aspect():index>=0 && index<7?__atomic_load_n(&game_values[index],__ATOMIC_RELAXED):-1;}
int native_setting_volume(int effects){return __atomic_load_n(&volumes[effects!=0],__ATOMIC_RELAXED);}
static void save(void) {
    if(!settings_path[0])return;
    char temporary[520];snprintf(temporary,sizeof temporary,"%s.tmp",settings_path);
    FILE *f=fopen(temporary,"w");if(!f)return;
    int ok=fprintf(f,"EOGSETTINGS1\ncontrol_style=%d\ncrosshair_colour=%d\nwatch_colour=%d\naspect=%d\nn64_aa=%d\nfps_limit=%d\nlook_sensitivity=%d\nsharpness=%d\nvignette=%d\nbloom=%d\nfilm_grain=%d\nwatch_skin=%d\nglass_detail=%d\nlod_strength=%d\nview_distance=%d\nvignette_mode=%d\n",
                   native_setting_style(),native_setting_crosshair(),native_setting_watch(),native_setting_aspect(),native_setting_aa(),native_setting_fps(),native_setting_sensitivity(),native_setting_sharpness(),native_setting_vignette(),native_setting_bloom(),native_setting_grain(),native_setting_skin(),native_setting_glass(),native_setting_lod(),native_setting_view_distance(),native_setting_vignette_mode())>0;
    if(fprintf(f,"watch_console_time=%d\n",native_setting_watch_clock())<0)ok=0;
    if(fprintf(f,"crosshair_opacity=%d\n",native_setting_crosshair_opacity())<0)ok=0;
    if(fprintf(f,"crosshair_shape=%d\n",native_setting_crosshair_shape())<0)ok=0;
    if(fprintf(f,"gyro_space=%d\ngyro_axis=%d\naim_lock=%d\nstamina_sway=%d\nsteady_scope=%d\n",native_setting_gyro_space(),native_setting_gyro_axis(),native_setting_aim_lock(),native_setting_stamina_sway(),native_setting_steady_scope())<0)ok=0;
    if(fprintf(f,"gyro_filter=%d\nalways_crosshair=%d\nvertical_sensitivity=%d\n",native_setting_gyro_filter(),native_setting_always_crosshair(),native_setting_vertical_sensitivity())<0)ok=0;
    if(fprintf(f,"aim_movement=%d\n",native_setting_aim_movement())<0)ok=0;
    if(fprintf(f,"flick_stick=%d\n",native_setting_flick())<0)ok=0;
    if(fprintf(f,"stereo_auto=%d\n",native_setting_stereo_auto())<0)ok=0;
    if(fprintf(f,"stereo_convergence=%d\n",native_setting_stereo_convergence())<0)ok=0;
    if(fprintf(f,"stereo_comfort=%d\n",native_setting_stereo_comfort())<0)ok=0;
    if(fprintf(f,"stereo_hud_depth=%d\n",native_setting_stereo_hud_depth())<0)ok=0;
    if(fprintf(f,"stereo_crosshair_hud=%d\n",native_setting_stereo_crosshair_hud())<0)ok=0;
    if(fprintf(f,"stereo_ghost_contrast=%d\n",native_setting_stereo_ghost_contrast())<0)ok=0;
    if(fprintf(f,"stereo_black_floor=%d\n",native_setting_stereo_black_floor())<0)ok=0;
    if(fprintf(f,"stereo_strength=%d\n",native_setting_stereo_strength())<0)ok=0;
    if(fprintf(f,"touch_sensitivity=%d\n",native_setting_touch_sensitivity())<0)ok=0;
    if(fprintf(f,"gyro_mode=%d\ngyro_sensitivity=%d\n",native_setting_gyro(),native_setting_gyro_sensitivity())<0)ok=0;
    if(fprintf(f,"audio_mixer=%d\n",native_setting_audio_mixer())<0)ok=0;
    if(fprintf(f,"guard_visibility=%d\nguard_ai_hz=%d\n",native_setting_guard_visibility(),native_setting_guard_ai_hz())<0)ok=0;
    if(fprintf(f,"randomiser=%d\nrandomiser_seed=%d\n",native_setting_randomiser(),native_setting_randomiser_seed())<0)ok=0;
    if(fprintf(f,"subsistence=%d\n",native_setting_subsistence())<0)ok=0;
    if(fprintf(f,"unlock_everything=%d\n",native_setting_unlock_all())<0)ok=0;
    for(int i=0;i<7;i++)if(fprintf(f,"game_option_%d=%d\n",i,native_setting_game(i))<0)ok=0;
    if(fprintf(f,"music_volume=%d\neffects_volume=%d\n",native_setting_volume(0),native_setting_volume(1))<0)ok=0;
    if(fclose(f))ok=0;
    if(!ok){remove(temporary);return;}
    if(rename(temporary,settings_path)==0)return;
    /* SD filesystems may reject renaming over an existing destination. */
    char backup[520];snprintf(backup,sizeof backup,"%s.bak",settings_path);
    remove(backup);
    if(rename(settings_path,backup)!=0)return;
    if(rename(temporary,settings_path)==0)remove(backup);
    else rename(backup,settings_path);
}
static void update(int *field,int value,int minimum,int maximum) {
    if(value<minimum || value>maximum)return;
    if(__atomic_exchange_n(field,value,__ATOMIC_RELAXED)!=value)save();
}
void native_setting_set_guard_visibility(int v){update(&guard_visibility,v,0,1);}
void native_setting_set_guard_ai_hz(int v){if(v==30 || v==15 || v==5)update(&guard_ai_hz,v,5,30);}
void native_setting_set_randomiser(int v){update(&randomiser,v,0,1);}
void native_setting_set_randomiser_seed(int v){update(&randomiser_seed,v,1,999999);}
void native_setting_set_subsistence(int v){update(&subsistence,v,0,1);}
void native_setting_set_unlock_all(int v){update(&unlock_all,v,0,1);}
void native_setting_set_audio_mixer(int v){update(&audio_mixer,v,0,3);}
void native_setting_set_skin(int v){update(&skin,v,0,4);}
void native_setting_set_lod(int v){if(v==4 && native_setting_lod()!=4)native_setting_dynamic_lod(1);update(&lod,v,0,4);}
void native_setting_set_vignette_mode(int v){update(&vignette_mode,v,0,3);}
void native_setting_set_view_distance(int v){update(&view_distance,v,0,3);}
void native_setting_set_glass(int v){update(&glass,v,0,1);}
void native_setting_set_style(int v){update(&style,v,0,9);}
void native_setting_set_crosshair(int v){update(&crosshair,v,0,6);}
void native_setting_set_watch(int v){update(&watch_colour,v,0,6);}
void native_setting_set_aspect(int v){update(&aspect,v,0,2);}
void native_setting_set_vignette(int v){update(&vignette,v,0,2);}
void native_setting_set_grain(int v){update(&grain,v,0,2);}
void native_setting_set_bloom(int v){update(&bloom,v,0,2);}
void native_setting_set_sharpness(int v){update(&sharpness,v,0,6);}
void native_setting_set_touch_sensitivity(int v){if(v%25==0)update(&touch_sensitivity,v,25,1000);}
void native_setting_set_gyro_space(int v){update(&gyro_space,v,0,2);}
void native_setting_set_gyro_axis(int v){update(&gyro_axis,v,0,1);}
void native_setting_set_aim_lock(int v){update(&aim_lock,v,0,1);}
void native_setting_set_stamina_sway(int v){update(&stamina_sway,v,0,1);}
void native_setting_set_steady_scope(int v){update(&steady_scope,v,0,1);}
void native_setting_set_aim_movement(int v){update(&aim_movement,v,0,1);}
void native_setting_set_stereo_auto(int v){if(v%1==0)update(&stereo_auto,v,0,1);}
void native_setting_set_stereo_convergence(int v){if(v%10==0)update(&stereo_convergence,v,10,200);}
void native_setting_set_stereo_comfort(int v){if(v%5==0)update(&stereo_comfort,v,-20,30);}
void native_setting_set_stereo_hud_depth(int v){if(v%5==0)update(&stereo_hud_depth,v,0,100);}
void native_setting_set_stereo_crosshair_hud(int v){if(v%1==0)update(&stereo_crosshair_hud,v,0,1);}
void native_setting_set_stereo_ghost_contrast(int v){if(v%5==0)update(&stereo_ghost_contrast,v,50,100);}
void native_setting_set_stereo_black_floor(int v){if(v%1==0)update(&stereo_black_floor,v,0,20);}
void native_setting_set_stereo_strength(int v){if(v%25==0)update(&stereo_strength,v,0,200);}
void native_setting_set_gyro_filter(int v){update(&gyro_filter,v,0,1);}
void native_setting_set_always_crosshair(int v){update(&always_crosshair,v,0,1);}
void native_setting_set_vertical_sensitivity(int v){if(v%25==0)update(&vertical_sensitivity,v,25,1000);}
void native_setting_set_flick(int v){update(&flick_stick,v,0,1);}
void native_setting_set_gyro(int v){update(&gyro_mode,v,0,2);}
void native_setting_set_gyro_sensitivity(int v){if(v%25==0)update(&gyro_sensitivity,v,25,400);}
void native_setting_set_sensitivity(int v){if(v%25==0)update(&sensitivity,v,25,1000);}
void native_setting_set_fps(int v){if(v==30 || v==60)update(&fps_limit,v,30,60);}
void native_setting_set_aa(int v){update(&aa,v,0,1);}
void native_setting_set_game(int index,int v){if(index==7)native_setting_set_aspect(v);else if(index>=0 && index<7)update(&game_values[index],v,0,index==6?2:1);}
void native_setting_set_volume(int effects,int v){update(&volumes[effects!=0],v,0,32767);}
void native_setting_set_watch_clock(int v){update(&watch_clock,v,0,1);}
void native_setting_set_crosshair_shape(int v){update(&crosshair_shape,v,0,7);}
void native_setting_set_crosshair_opacity(int v){if(v==-1 || v%10==0)update(&crosshair_opacity,v,-1,100);}
void native_settings_init(const char *directory) {
    watch_clock=0;
    crosshair_opacity=-1;
    crosshair_shape=0;
    gyro_space=gyro_axis=aim_lock=stamina_sway=0;steady_scope=1;
    stereo_auto=1;stereo_convergence=100;stereo_comfort=0;stereo_hud_depth=75;stereo_crosshair_hud=0;stereo_ghost_contrast=100;stereo_black_floor=0;
    aim_movement=0;stereo_strength=100;
    gyro_filter=always_crosshair=0;vertical_sensitivity=250;flick_stick=0;gyro_mode=0;gyro_sensitivity=250;touch_sensitivity=250;
    randomiser=0;randomiser_seed=1;subsistence=0;guard_visibility=0;guard_ai_hz=30;unlock_all=0;audio_mixer=1;dynamic_lod=1;vignette_mode=0;render_state=0;
    style=-1;crosshair=watch_colour=0;aspect=-1;aa=1;fps_limit=60;sensitivity=250;sharpness=vignette=bloom=grain=skin=0;glass=1;lod=1;view_distance=2;settings_path[0]=0;config_missing=0;
    for(int i=0;i<7;i++)game_values[i]=-1;
    volumes[0]=volumes[1]=-1;
    if(!directory || snprintf(settings_path,sizeof settings_path,"%s/native-options.ini",directory)>=(int)sizeof settings_path){settings_path[0]=0;return;}
    FILE *f=fopen(settings_path,"r");
    if(!f && errno==ENOENT) {
        /* Recover the last complete file if power/exit interrupted FAT's
         * backup-and-replace step. Its presence is not a first launch. */
        char backup[520];snprintf(backup,sizeof backup,"%s.bak",settings_path);
        if(rename(backup,settings_path)==0)f=fopen(settings_path,"r");
    }
    if(!f){config_missing=errno==ENOENT;return;}
    char line[128]={0};int s=-1,c=0,w=0,a=-1,q=1,fps=60,sens=250,sharp=0,vig=0,glow=0,noise=0,watchskin=0,glassdetail=1,lodstrength=1,viewdistance=2,vigmode=0;
    int load_watch_clock=0,filter=0,persistent=0,vertical=-1,load_crosshair_shape=0,load_crosshair_opacity=-1;
    int load_stereo_auto=1,load_stereo_convergence=100,load_stereo_comfort=0,load_stereo_hud_depth=75,load_stereo_crosshair_hud=0,load_stereo_ghost_contrast=100,load_stereo_black_floor=0;
    int strength=100;
    int space=0,axis=0,lock=0,sway=0,steady=1,aimmove=0;
    int flick=0,touchsens=250,gyromode=0,gyrosens=250,mixer=1,unlocked=0,guardvisible=0,guardhz=30,subs=0,rando=0,seed=1;
    int games[7]={-1,-1,-1,-1,-1,-1,-1},music=-1,effects=-1;
    int ok=fgets(line,sizeof line,f)!=NULL && !memcmp(line,"EOGSETTINGS1\n",13);
    unsigned found=0;
    while(ok && fgets(line,sizeof line,f)) {
        if(sscanf(line,"control_style=%d",&s)==1)found|=1;
        else if(sscanf(line,"crosshair_colour=%d",&c)==1)found|=2;
        else if(sscanf(line,"watch_console_time=%d",&load_watch_clock)==1){}
        else if(sscanf(line,"crosshair_opacity=%d",&load_crosshair_opacity)==1){}
        else if(sscanf(line,"crosshair_shape=%d",&load_crosshair_shape)==1){}
        else if(sscanf(line,"watch_colour=%d",&w)==1)found|=4;
        else if(sscanf(line,"aspect=%d",&a)==1)found|=8;
        else if(sscanf(line,"n64_aa=%d",&q)==1)found|=16;
        else if(sscanf(line,"vignette=%d",&vig)==1){}
        else if(sscanf(line,"vignette_mode=%d",&vigmode)==1){}
        else if(sscanf(line,"film_grain=%d",&noise)==1){}
        else if(sscanf(line,"watch_skin=%d",&watchskin)==1){}
        else if(sscanf(line,"glass_detail=%d",&glassdetail)==1){}
        else if(sscanf(line,"lod_strength=%d",&lodstrength)==1){}
        else if(sscanf(line,"view_distance=%d",&viewdistance)==1){}
        else if(sscanf(line,"bloom=%d",&glow)==1){}
        else if(sscanf(line,"sharpness=%d",&sharp)==1){}
        else if(sscanf(line,"stereo_auto=%d",&load_stereo_auto)==1){}
        else if(sscanf(line,"stereo_convergence=%d",&load_stereo_convergence)==1){}
        else if(sscanf(line,"stereo_comfort=%d",&load_stereo_comfort)==1){}
        else if(sscanf(line,"stereo_hud_depth=%d",&load_stereo_hud_depth)==1){}
        else if(sscanf(line,"stereo_crosshair_hud=%d",&load_stereo_crosshair_hud)==1){}
        else if(sscanf(line,"stereo_ghost_contrast=%d",&load_stereo_ghost_contrast)==1){}
        else if(sscanf(line,"stereo_black_floor=%d",&load_stereo_black_floor)==1){}
        else if(sscanf(line,"stereo_strength=%d",&strength)==1){}
        else if(sscanf(line,"gyro_filter=%d",&filter)==1){}
        else if(sscanf(line,"always_crosshair=%d",&persistent)==1){}
        else if(sscanf(line,"vertical_sensitivity=%d",&vertical)==1){}
        else if(sscanf(line,"look_sensitivity=%d",&sens)==1){}
        else if(sscanf(line,"gyro_space=%d",&space)==1){}
        else if(sscanf(line,"gyro_axis=%d",&axis)==1){}
        else if(sscanf(line,"aim_movement=%d",&aimmove)==1){}
        else if(sscanf(line,"aim_lock=%d",&lock)==1){}
        else if(sscanf(line,"stamina_sway=%d",&sway)==1){}
        else if(sscanf(line,"steady_scope=%d",&steady)==1){}
        else if(sscanf(line,"flick_stick=%d",&flick)==1){}
        else if(sscanf(line,"touch_sensitivity=%d",&touchsens)==1){}
        else if(sscanf(line,"gyro_mode=%d",&gyromode)==1){}
        else if(sscanf(line,"gyro_sensitivity=%d",&gyrosens)==1){}
        else if(sscanf(line,"fps_limit=%d",&fps)==1){}
        else if(sscanf(line,"music_volume=%d",&music)==1){}
        else if(sscanf(line,"effects_volume=%d",&effects)==1){}
        else if(sscanf(line,"guard_visibility=%d",&guardvisible)==1){}
        else if(sscanf(line,"guard_ai_hz=%d",&guardhz)==1){}
        else if(sscanf(line,"randomiser=%d",&rando)==1){}
        else if(sscanf(line,"randomiser_seed=%d",&seed)==1){}
        else if(sscanf(line,"subsistence=%d",&subs)==1){}
        else if(sscanf(line,"unlock_everything=%d",&unlocked)==1){}
        else if(sscanf(line,"audio_mixer=%d",&mixer)==1){}
        else {int index,value;if(sscanf(line,"game_option_%d=%d",&index,&value)==2 && index>=0 && index<7)games[index]=value;}
    }
    ok=ok && !ferror(f) && (found&15)==15 && s>=-1 && s<=9 && c>=0 && c<=6 && w>=0 && w<=6 && a>=-1 && a<=2 && q>=0 && q<=1;
    if(load_watch_clock<0 || load_watch_clock>1)ok=0;
    if(load_crosshair_opacity!=-1 && (load_crosshair_opacity<0 || load_crosshair_opacity>100 || load_crosshair_opacity%10))ok=0;
    if(load_crosshair_shape<0 || load_crosshair_shape>7)ok=0;
    if(guardvisible<0 || guardvisible>1 || (guardhz!=30 && guardhz!=15 && guardhz!=5))ok=0;
    if(rando<0 || rando>1 || seed<1 || seed>999999)ok=0;
    if(subs<0 || subs>1 || unlocked<0 || unlocked>1 || mixer<0 || mixer>3)ok=0;
    if(fps!=30 && fps!=60)ok=0;
    if(space<0 || space>2 || axis<0 || axis>1 || lock<0 || lock>1 || sway<0 || sway>1 || steady<0 || steady>1)ok=0;
    if(aimmove<0 || aimmove>1)ok=0;
    if(vertical==-1)vertical=sens; /* Older files used one value for both axes. */
    if(filter<0 || filter>1 || persistent<0 || persistent>1 || vertical<25 || vertical>1000 || vertical%25)ok=0;
    if(flick<0 || flick>1)ok=0;
    if(load_stereo_auto<0 || load_stereo_auto>1 || load_stereo_auto%1)ok=0;
    if(load_stereo_convergence<10 || load_stereo_convergence>200 || load_stereo_convergence%10)ok=0;
    if(load_stereo_comfort<-20 || load_stereo_comfort>30 || load_stereo_comfort%5)ok=0;
    if(load_stereo_hud_depth<0 || load_stereo_hud_depth>100 || load_stereo_hud_depth%5)ok=0;
    if(load_stereo_crosshair_hud<0 || load_stereo_crosshair_hud>1 || load_stereo_crosshair_hud%1)ok=0;
    if(load_stereo_ghost_contrast<50 || load_stereo_ghost_contrast>100 || load_stereo_ghost_contrast%5)ok=0;
    if(load_stereo_black_floor<0 || load_stereo_black_floor>20 || load_stereo_black_floor%1)ok=0;
    if(strength<0 || strength>200 || strength%25)ok=0;
    if(touchsens<25 || touchsens>1000 || touchsens%25)ok=0;
    if(gyromode<0 || gyromode>2 || gyrosens<25 || gyrosens>1000 || gyrosens%25)ok=0;
    if(sens<25 || sens>1000 || sens%25 || sharp<0 || sharp>6 || vig<0 || vig>2 || glow<0 || glow>2 || noise<0 || noise>2 || watchskin<0 || watchskin>4 || glassdetail<0 || glassdetail>2 || lodstrength<0 || lodstrength>4 || viewdistance<0 || viewdistance>3 || vigmode<0 || vigmode>3)ok=0;
    for(int i=0;i<7;i++)if(games[i]<-1 || games[i]>(i==6?2:1))ok=0;
    if(music< -1 || music>32767 || effects< -1 || effects>32767)ok=0;
    fclose(f);
    if(ok){watch_clock=load_watch_clock;crosshair_opacity=load_crosshair_opacity;crosshair_shape=load_crosshair_shape;stereo_auto=load_stereo_auto;stereo_convergence=load_stereo_convergence;stereo_comfort=load_stereo_comfort;stereo_hud_depth=load_stereo_hud_depth;stereo_crosshair_hud=load_stereo_crosshair_hud;stereo_ghost_contrast=load_stereo_ghost_contrast;stereo_black_floor=load_stereo_black_floor;gyro_filter=filter;always_crosshair=persistent;vertical_sensitivity=vertical;stereo_strength=strength;aim_movement=aimmove;gyro_space=space;gyro_axis=axis;aim_lock=lock;stamina_sway=sway;steady_scope=steady;flick_stick=flick;touch_sensitivity=touchsens;gyro_mode=gyromode;gyro_sensitivity=gyrosens>400?400:gyrosens;randomiser=rando;randomiser_seed=seed;subsistence=subs;guard_visibility=guardvisible;guard_ai_hz=guardhz;unlock_all=unlocked;audio_mixer=mixer;style=s;crosshair=c;watch_colour=w;aspect=a;aa=q;fps_limit=fps;sensitivity=sens;sharpness=sharp;vignette=vig;vignette_mode=vigmode;bloom=glow;grain=noise;skin=watchskin;glass=glassdetail==2?1:glassdetail;lod=lodstrength;view_distance=viewdistance;memcpy(game_values,games,sizeof games);volumes[0]=music;volumes[1]=effects;}
}
int native_settings_apply_console_defaults(int model) {
    /* Existence, not parse success, controls first-launch detection. Never
     * replace an existing user's choices, even when their file is invalid. */
    if(!config_missing)return 0;
    int newer=native_console_is_new(model);
    style=newer?8:9;aspect=newer?1:0;fps_limit=30;aa=0;
    game_values[0]=1;sharpness=4;vignette=1;vignette_mode=1;
    glass=newer?0:1;lod=newer?1:4;view_distance=newer?1:3;
    config_missing=0;
    save();
    return 1;
}
const char *native_colour_name(int is_watch,int value) {
    static const char *const watch_names[]={"Green\n","White\n","Amber\n","Red\n","Blue\n","Gold\n","Purple\n"};
    static const char *const sight_names[]={"Original\n","White\n","Green\n","Amber\n","Red\n","Blue\n","Purple\n"};
    if(is_watch)return watch_names[value>=0 && value<7?value:0];
    return sight_names[value>=0 && value<7?value:0];
}
const char *native_string(unsigned id) {
    switch(id) {
    case 0xf000:return "1.5 Vesper\n";
    case 0xf001:return "1.6 Jinx\n";
    case 0xf010:return "Crosshair\n";
    case 0xf011:return "Watch colour\n";
    case 0xf013:return "FPS limit\n";
    case 0xf012:return "Supersampling\n";
    default:return NULL;
    }
}

/* UI order is independent of persistent option identifiers. */
int native_menu_option_at(int index) {
    static const unsigned char rows[]={0,15,42,31,29,30,33,34,41,35,32,39,36,37,38,43,3,4,5,6,7,9,14,10,13,22,23,21,27,28,40,44,45,46,47,48,49,50,8,11,16,17,24,18,19,12,20,1,2,25,26,51,52,53};
    return index>=0 && index<(int)sizeof rows?rows[index]:-1;
}

/* Frontend categories contain each shared option exactly once. Watch pages
 * still use the same option IDs and setters, so preferences remain shared. */
#include "native_menu_options.h"
static const int native_category_rows[][17]={
    {0,3,4,5,7,15,42,31,29,30,33,34,41,35,32,39,-1},
    {9,10,14,13,-1},
    {22,23,21,-1},
    {16,17,24,18,19,-1},
    {40,44,45,46,47,48,49,50,-1},
    {1,2,25,-1},
    {6,8,51,11,52,12,20,-1},
    {36,37,38,43,-1},
    {27,28,53,26,-1}
};
int native_menu_category_count(int category){
    if(category<0 || category>=NATIVE_MENU_CATEGORY_COUNT)return NATIVE_MENU_CATEGORY_COUNT;
    int count=0;while(count<17 && native_category_rows[category][count]>=0)count++;
    return count;
}
int native_menu_category_option(int category,int index){
    if(category<0 || category>=NATIVE_MENU_CATEGORY_COUNT || index<0 || index>=native_menu_category_count(category))return -1;
    return native_category_rows[category][index];
}
const char *native_menu_category_title(int category){
    static const char *const titles[]={"CONTROLS\n","DISPLAY\n","GRAPHICS\n","SCREEN EFFECTS\n","3D SETTINGS\n","AUDIO\n","INTERFACE & WATCH\n","ACCESSIBILITY\n","EXPERIMENTAL\n"};
    return category>=0 && category<NATIVE_MENU_CATEGORY_COUNT?titles[category]:"OPTIONS\n";
}
