# Eyes of Gold Launcher

A work-in-progress native port of GoldenEye 007 for Nintendo 3DS, based on the [GoldenEye decompilation and PC port](https://github.com/jkdansereau/goldeneye-pc-port).

All 20 campaign missions are available, along with 11 multiplayer maps. Multiplayer is only a preview for now: you can control one player while the second stays idle. There is no networking yet.

The game is playable, but there are still bugs and performance issues. New 3DS systems handle it better; busy scenes can be slow on an Old 3DS.

## What the port adds

The original campaign, menus, intros and watch run through the native engine. On top of that, the port adds features for the 3DS:

- **Dual-screen support.** The bottom screen has touch controls for menus and a full-screen version of the watch interface. During gameplay it shows your mission objectives, and you can drag on it to look around.
- **Stereoscopic UI.** The 3D slider adds depth to supported menu and HUD elements, including the watch interface in front of the watch model. The level itself is still rendered flat; this is not full stereoscopic world rendering.
- **Two extra control styles.** **1.5 Vesper** uses the Circle Pad for movement and the C-stick for looking. **1.6 Jinx** uses the Circle Pad for movement, the face buttons for looking and the D-pad for the actions moved from those buttons. The original control styles are still available, and look sensitivity is adjustable.
- **Display options.** Choose 4:3, 5:4 or the top screen's native 5:3 aspect ratio, with a 30 or 60 FPS cap. These are limits, not guaranteed frame rates. There is also optional supersampling and a choice of smoothing or sharpening filters.
- **Performance settings.** Adjust model and texture detail, view distance, glass and effects. Dynamic LOD changes the detail level as the frame rate changes. Experimental guard settings can reduce update rates and work spent on distant or hidden guards, but may affect their behaviour.
- **Extra screen effects.** Optional bloom, film grain and vignette. The vignette can stay on or appear only while crouching, aiming, or either.
- **Watch and crosshair customisation.** Choose interface and crosshair colours, including blue and gold, along with alternate watch-hand styles.
- **Audio options.** Music and sound effects come from the ROM. You can choose CPU or DSP-assisted mixing; the DSP option still leaves some audio work on the CPU.
- **Saved settings.** Port-specific options are stored in `native-options.ini`. First-launch defaults depend on the console model and do not replace an existing settings file.
- **Setup on the console.** Asset conversion runs on the 3DS, so players do not need a PC to prepare their ROM. The boot screen shows setup progress.

The main menu also has an options page, an exit button and an unlock-everything option for levels and cheats. Most in-game settings are available through the watch.

## Playing the game

You need a homebrew-enabled 3DS and your own legally backed-up **US retail GoldenEye 007 ROM**. Game assets are read from that ROM and prepared on the console the first time you launch it.

1. Copy the `3ds` folder from the install ZIP to the root of your SD card.
2. Put your ROM in `sd:/3ds/eyes-of-gold-native-game/`, named `GoldenEye.n64`, `GoldenEye.z64` or `GoldenEye.v64`.
3. Open **Eyes of Gold Launcher** in the Homebrew Launcher.

Leave at least 48 MiB free on the SD card for setup. The first launch takes longer because it prepares the game files. Later launches reuse those files. Keep the ROM in the folder in case an update needs to rebuild them.

This repository contains the source. If you want to build your own install ZIP, follow the instructions below.

## Building from source

The build scripts currently expect devkitPro at `/opt/devkitpro`. Install its 3DS tools, including devkitARM, libctru, Citro3D, picasso, smdhtool and 3dsxtool. You also need Python 3, GCC/G++, make, Git and the Python package `pyelftools`. Pillow is only used for optional image previews.

Install the Python build dependency in your environment with:

```sh
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
```

Fetch the engine source:

```sh
python3 tools/setup.py
```

The setup script downloads [goldeneye-pc-port](https://github.com/jkdansereau/goldeneye-pc-port) at commit `fda76b69837e6c5aae436e0d3e798989e49b06d2`. The port depends on that version, so the script will stop if an existing checkout has a different revision or local edits.

Put your US ROM at `local/roms/GoldenEye.n64`, then run:

```sh
make
```

The install ZIP will be written to:

```text
local/releases/eyes-of-gold-launcher.zip
```

The tools accept all three common N64 ROM byte orders. After conversion to big endian, the expected SHA-1 is `abe01e4aeb033b6c0836819f549c791b26cfde83`.

The build creates test data from your ROM, compiles the engine, runs its checks and checks for embedded game assets before packaging. Generated files go into `local/` and `native-engine/build/`. Those folders and the downloaded upstream source are ignored by Git.

An isolated checkout has been built successfully with the installed devkitPro tools and a user-supplied ROM. The build checks, executable-data audit and install ZIP creation passed. See [the build test notes](docs/clean-build-test.md) for details.

## Finding your way around

- `native-engine/src/` and `include/` contain the 3DS code: graphics, audio, input, menus and asset loading.
- `native-engine/tools/` contains the build scripts, engine patches and conversion tools.
- `native-engine/tests/` contains tests that run on a PC.
- The layout and review JSON files describe where data is stored in the ROM and how the build checks it.
- `native-engine/distribution/` contains the instructions and license notices included with a build.

More details: [ROM conversion](native-engine/CONSOLE-ASSET-CONVERSION.md), [asset checks](docs/rom-data-release.md), and [what is included in this repository](SOURCE-RELEASE.md).

## Credits and licenses

This project builds on the work of the GoldenEye decompilation and PC-port contributors. Component credits and license sources are listed in [SOURCES.txt](native-engine/distribution/licenses/SOURCES.txt).

The original Eyes of Gold additions are MIT licensed. The inherited engine code and third-party libraries have separate terms; see [NOTICE.md](NOTICE.md) and the [license guide](native-engine/distribution/LICENSE.txt).

Eyes of Gold is an unofficial fan project and is not affiliated with Nintendo, Rare or the James Bond rights holders.
