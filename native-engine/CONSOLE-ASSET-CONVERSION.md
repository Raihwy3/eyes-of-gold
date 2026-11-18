# Preparing game files on the 3DS

Players do not need a PC conversion tool. The launcher reads the user's ROM and prepares the files it needs on the console.

Put a legally backed-up **12 MiB US retail ROM** in `sd:/3ds/eyes-of-gold-native-game/`, alongside the executable. Name it `GoldenEye.n64`, `GoldenEye.z64` or `GoldenEye.v64`.

Startup detects the byte order from the file itself, rather than its extension. It converts the ROM to big endian and checks the whole file against SHA-1 `abe01e4aeb033b6c0836819f549c791b26cfde83`. Unsupported regions, modified ROMs and incomplete dumps are rejected.

## First launch

The launcher checks for four files and creates any that are missing or invalid:

| File | Contents |
| --- | --- |
| `native-cart.bin` | The ROM in the byte order expected by the loader |
| `native-game.dat` | Models and stage data converted for the port |
| `native-audio.pcm` | Decoded samples from the game's sound banks |
| `native-render.dat` | Prepared rendering data, including LODs, title graphics and the bottom-screen background |

All 20 campaign missions and 11 multiplayer arenas are enabled in the current build. Some older conversion notes referred only to Dam; that restriction no longer applies.

Setup happens before the engine and its graphics/audio workers start. The screens show progress, and B cancels. Leave at least 48 MiB free on the SD card. There is no reliable hardware timing estimate yet.

Files are written to temporary paths and renamed when complete. If setup is cancelled or power is lost, the next launch retries unfinished work. Completed files are reused.

## Later launches

Once these files are valid, the game can load them without converting everything again. Keep the ROM in the folder so updates or repairs can rebuild the caches when needed.

To test setup from scratch, move the four generated files somewhere safe and launch again with the ROM present. You can keep `native-options.ini` to preserve your settings.

## Data loaded at startup

The executable also has empty storage for original weapon, skeleton, animation and AI tables, menu text, colours and other engine records. Startup fills that storage from the ROM-derived cartridge file. These records are not bundled as populated arrays in the release executable.

The build compares the console conversion code with PC-generated references. It checks converted entries, audio samples, rendering data, byte-order handling, cache validation and cancellation. The packaging check also looks for original game data left in the executable. See [the asset checks](../docs/rom-data-release.md) for details.
