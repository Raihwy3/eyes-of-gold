# Native 3DS engine

This folder contains the 3DS platform code, engine patches, conversion tools and tests. See the [main README](../README.md) for installation and build instructions.

The build uses a fixed version of the upstream engine. `tools/build.py` writes patched copies into `build/`, leaving the upstream checkout unchanged. `tools/build_game.py` builds the launcher, runs the checks and packages the install ZIP.

The `build/` folder is generated and should stay out of Git.

See [ROM conversion](CONSOLE-ASSET-CONVERSION.md) for how game files are prepared on the console, and [NOTICE.md](NOTICE.md) for license details.
