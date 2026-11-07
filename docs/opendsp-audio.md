# Experimental OpenDSP audio

Select **Mixer (restart)** in the watch Audio page or the main options, then exit and relaunch:

| Mode | Firmware | Dry mixing | Reverb |
|---|---|---|---|
| CPU | User's existing DSP firmware | CPU | Original CPU |
| NDSP | User's existing DSP firmware | Up to 23 DSP voices; overflow on CPU | Original CPU |
| OpenDSP | Embedded open firmware | Same 23-voice path | Original CPU |
| OpenDSP FX | Embedded open firmware | 22 DSP voices; overflow on CPU | OpenDSP reverb |

The default stays NDSP and existing saved values 0/1 retain their meaning. `cpu-audio.flag` overrides the selection. OpenDSP is loaded for this process; it does not flash or replace the console's system firmware. OpenDSP modes don't need a dumped dspfirm.cdc. If OpenDSP initialization fails, the launcher tries the original NDSP path; that fallback still needs the user's existing firmware setup. Failed DSP reverb initialization preserves original CPU effects.

FX mode reserves channel 23 for an isolated, already-enveloped stereo wet send. Channel 0 retains CPU dry overflow; channels 1–22 mix dry voices. The original FX pull collects voices first, then bypasses its CPU delay/filter processing only when the complete subframe can be captured. Otherwise it runs the original CPU effect. If staging or linear allocation fails, the original CPU mix remains enabled. Wet sends keep the game's envelope and send level; OpenDSP's room algorithm is an approximation, with a fixed mono-width 1.2-second preset, not the original N64 effect or a mapping of its individual delay coefficients.

Pitch resampling, envelopes, panning, sequencing and voice management remain on the CPU. Existing ROM-derived PCM caching is unchanged. DSP reverb and voices share the same DSP budget; a performance improvement has not been measured on hardware.

With the existing logging flag enabled, output includes the active firmware/mode, CPU `Audio work` and existing dry-mix fallback counters. Periodic OpenDSP mailbox statistics requests have been removed from playback to avoid blocking the audio callback. Compare sound and CPU audio time in the same location with the same music/voices; a higher FPS is not proof of uninterrupted audio.

Audio synthesis completes synchronously in `alAudioFrame`. The native producer now posts its completion directly to its original reply queue instead of submitting an already-completed audio task to the shared N64 RSP scheduler. This avoids waiting for a slow graphics task before starting the next audio block. The original audio tick rate, three-buffer pipeline and graphics task completions remain in place. This applies to every mixer mode; it does not make synthesis independent of CPU availability or scheduler retrace messages.

The audio worker now runs at priority 0x30, above the game/render workers at 0x3f. The queue regulator reserves 1536 stereo frames (about 70 ms at 22050 Hz), up from 1024, to absorb short scheduling delays. This adds approximately 23 ms of buffering and does not change the sample rate, synthesis or sound quality. A queue slot that stays busy for 2000 one-millisecond waits reports an audio-stall error instead of waiting forever; live DSP buffers are never overwritten. These changes apply to CPU, NDSP and OpenDSP output. They do not guarantee playback through sustained CPU overload.

OpenDSP source and its prebuilt open firmware are pinned to `a15c8615c1659f34bfb2b38df69a9860f4cc7080`, verified against the vendor manifest before building, and licensed under BSD 2-Clause. Build the in-tree library with devkitPro; no system SDK installation is modified. Its authorship and full licence are packaged with the launcher.
