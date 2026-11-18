# CIA build

This is a standalone installed application, rather than a forwarder to the 3DSX.

Install `eyes-of-gold-launcher-0.9.5.cia` with FBI and launch Eyes of Gold Launcher from HOME Menu. Keep the existing `/3ds/eyes-of-gold-native-game/` folder on the SD card. ROMs, settings, saves and converted assets use the same absolute paths as the 3DSX; there is no separate CIA data folder. An existing installation does not need its assets prepared again just because it uses CIA.

A fresh installation still needs a legally backed-up US retail ROM in that folder, named `GoldenEye.n64`, `GoldenEye.z64` or `GoldenEye.v64`.

The CIA currently needs manual updates by installing a newer CIA with the same title ID. The 3DSX startup updater is omitted from this build because replacing a 3DSX cannot update an installed title. The ordinary 3DSX build retains its updater. Both formats share `native-options.ini` and the save files.

## Building

Run `make cia` to build and validate the native engine, then package it. If the current native engine has already been built, `python3 native-engine/tools/build_cia.py` can package that build without repeating the full engine checks. It recompiles the CIA platform entry point and links a separate ELF; it does not overwrite the 3DSX release ZIP.

In addition to the normal toolchain, install Pillow, makerom and bannertool. Set `MAKEROM` and `BANNERTOOL` to their executable paths, put them on PATH, or place them in the ignored `local/cia-tools/` folder. Packaging does not download tools automatically.

The tested tools were:

- [makerom 0.19.0](https://github.com/3DSGuy/Project_CTR/releases/tag/makerom-v0.19.0), Ubuntu x86-64 ZIP SHA-256 `287b809dec064e0ad597e3d272c49ecb7eed41693d5ee6fef9d8a8aa24c2497e`.
- [bannertool 1.2.3](https://github.com/carstene1ns/3ds-bannertool/releases/tag/1.2.3), Linux tarball SHA-256 `748519d200519db18e9fd00c332ac32f5411e41230258291e89953a76c1f7155`.

Output is written to `local/releases/cia/`. The CIA ZIP uses the distinct name `eyes-of-gold-cia-VERSION.zip`, so the 3DSX updater will ignore it if both packages are attached to a release. The build creates the banner from the custom launcher icon, plain text and generated silence. It includes makerom’s MIT-licensed Homebrew boot logo in ExeFS, with its license notice. It includes no Nintendo boot logo, game-data RomFS, ROM or extracted game files.

## Application settings and validation

The title ID is `000400000E064D00`. Keep it stable for future CIA updates. `native-engine/cia/launcher.rsf` defines SD access, the required system services and the existing worker-core access. It requests the 80 MiB application mode on Old 3DS and 124 MiB on New 3DS; these are memory limits rather than a performance guarantee. Old 3DS extended-memory mode can restart HOME Menu when leaving the application.

The package check compares every ELF load segment with the installed `.code` payload, checks ExeFS hashes, verifies the icon/banner, homebrew boot-logo digest and title ID, and confirms that game paths still point to the shared SD folder. The original ROM-data audit also runs on the CIA ELF. Reports are in `native-engine/build/cia/`.

These checks do not replace a console boot, gameplay and HOME Menu exit test.

The first CIA test omitted the boot logo. The package validator now rejects that build and requires the verified Homebrew logo; whether this resolves the launch error still needs console testing.
