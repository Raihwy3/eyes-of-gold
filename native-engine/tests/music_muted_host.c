#include <ultra64.h>
#include "music.h"
#include "snd.h"
#include <stdlib.h>
#include <stdio.h>
extern s8 g_sndBootswitchSound;
extern u16 g_musicXTrack1Volume, g_musicXTrack2Volume, g_musicXTrack3Volume;
extern ALCSPlayer *g_musicXTrack1SeqPlayer, *g_musicXTrack2SeqPlayer, *g_musicXTrack3SeqPlayer;
extern void native_muted_audio_init(void);
extern void sndSetSfxSlotVolume(u8, u16);
extern u16 sndGetSfxSlotNaturalVolume(u8);
extern f32 g_sndSfxVolumeScale;
static int calls;
static int mask_depth;
void alCSPPlay(ALCSPlayer *player) { (void)player; abort(); }
void alCSPStop(ALCSPlayer *player) { (void)player; abort(); }
OSIntMask osSetIntMask(OSIntMask mask) {
    if (mask == OS_IM_NONE) { mask_depth++; return OS_IM_ALL; }
    if (mask == OS_IM_ALL) { if (--mask_depth < 0) abort(); return OS_IM_NONE; }
    abort();
}
void alEvtqPostEvent(ALEventQueue *queue, ALEvent *event, ALMicroTime delta) {
    (void)queue; (void)event; (void)delta;
    abort(); /* Muted reset must not submit an event. */
}
void alCSPSetVol(ALCSPlayer *player, s16 volume) {
    (void)volume;
    if (!player || g_sndBootswitchSound) abort();
    calls++;
}
int main(void) {
    g_sndBootswitchSound = 1;
    if (g_sndSfxSlotVolume || g_sndSfxSlotNaturalVolume) abort();
    native_muted_audio_init();
    for (unsigned i=0; i<SFX_SLOT_COUNT; ++i)
        if (sndGetSfxSlotNaturalVolume(i) != AL_SNDP_GROUP_VOLUME_MAX || g_sndSfxSlotVolume[i] != AL_SNDP_GROUP_VOLUME_MAX) abort();
    /* Same reset that lvlStageLoad runs before the music updates. */
    sndSetScalerApplyVolumeAllSfxSlot(1.0f);
    sndSetScalerApplyVolumeAllSfxSlot(0.5f);
    for (unsigned i=0; i<SFX_SLOT_COUNT; ++i)
        if (g_sndSfxSlotVolume[i] != 16383) abort();
    sndSetSfxSlotVolume(2, 1000);
    if (sndGetSfxSlotNaturalVolume(2) != 1000 || g_sndSfxSlotVolume[2] != 500) abort();
    sndApplyVolumeAllSfxSlot(2000);
    for (unsigned i=0; i<SFX_SLOT_COUNT; ++i)
        if (sndGetSfxSlotNaturalVolume(i) != 2000 || g_sndSfxSlotVolume[i] != 1000) abort();
    sndSetScalerApplyVolumeAllSfxSlot(1.0f);
    if (mask_depth || g_sndSfxVolumeScale != 1.0f) abort();
    puts("PASS original muted stage SFX volume reset, all slots, scaling and balanced critical sections");
    musicTrack1ApplySeqpVol(123);
    musicTrack2ApplySeqpVol(456);
    musicTrack3ApplySeqpVol(789);
    if (calls || g_musicXTrack1Volume != 123 || g_musicXTrack2Volume != 456 || g_musicXTrack3Volume != 789) abort();
    /* Watch opening/closing fades also run on the scheduler thread. */
    for (unsigned pass=0;pass<3;pass++) {
        musicTrack1FadeOut(0.1f); musicTrack2FadeOut(0.1f); musicTrack3FadeOut(0.1f);
        for(unsigned frame=0;frame<20;frame++) musicFadeTick();
        musicTrack1FadeIn(0.1f,123); musicTrack2FadeIn(0.1f,456); musicTrack3FadeIn(0.1f,789);
        for(unsigned frame=0;frame<20;frame++) musicFadeTick();
    }
    if (calls) abort();
    puts("PASS repeated muted watch/music fade-out, fade-in and scheduler ticks without player events");
    ALCSPlayer player = {0};
    g_musicXTrack1SeqPlayer = g_musicXTrack2SeqPlayer = g_musicXTrack3SeqPlayer = &player;
    g_sndBootswitchSound = 0;
    musicTrack1ApplySeqpVol(100);
    musicTrack2ApplySeqpVol(200);
    musicTrack3ApplySeqpVol(300);
    if (calls != 3) abort();
    puts("PASS muted music volume preserves settings without null-player events; enabled path still submits");
    return 0;
}
