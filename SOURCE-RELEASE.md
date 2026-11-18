# What is in this repository?

This is the source for the native 3DS port. It includes the platform code, build scripts, tests, custom launcher icons and the metadata needed to read game data from a ROM.

ROMs, extracted assets, converted game files, saves, logs and compiled builds are left out. The older engine recreation and development history are also left out.

The JSON layout files contain ROM offsets, field types, pointer mappings and checksums. The conversion tools need these to rebuild the game's data for the 3DS. They do not contain the model, texture or sound files themselves.

## Downloaded engine source

`tools/setup.py` downloads the upstream engine into `upstream/goldeneye-pc/`. That checkout includes inherited engine code and data declarations, so it stays out of this repository. The build creates patched copies under `native-engine/build/` and leaves the downloaded source unchanged.

The build also removes original asset data from the compiled objects and adds code to load it from the user's ROM. Downloading the upstream source separately does not replace that step.

## Keeping private files out of Git

`upstream/`, `local/` and `native-engine/build/` are ignored. Keep them that way, and check `git status --short` before committing after a build. Do not force-add your ROM, generated caches or downloaded source.

`source-inventory.json` records the files in this source export and their checksums. The export was checked for matching source files and valid Python syntax. An isolated checkout has since completed the full build and packaging checks; see [the build test notes](docs/clean-build-test.md).

The asset checks describe what the tools include in a build. They do not change the licenses of the inherited code. See [NOTICE.md](NOTICE.md) for the separate license terms.
