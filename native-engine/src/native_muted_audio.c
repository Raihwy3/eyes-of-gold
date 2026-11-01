/* Volume settings remain valid even when the audio heap is not created. */
#include <ultra64.h>
#include "music.h"
#include "snd.h"

void native_muted_audio_init(void)
{
    static s16 volume[SFX_SLOT_COUNT];
    static u16 natural_volume[SFX_SLOT_COUNT];
    for (unsigned i = 0; i < SFX_SLOT_COUNT; ++i) {
        volume[i] = AL_SNDP_GROUP_VOLUME_MAX;
        natural_volume[i] = AL_SNDP_GROUP_VOLUME_MAX;
    }
    /* Normal audio initialization replaces these with its own heap tables. */
    g_sndSfxSlotVolume = volume;
    g_sndSfxSlotNaturalVolume = natural_volume;
}
