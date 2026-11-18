# How game assets are kept out of the install ZIP

The launcher gets original game content from the user's verified ROM. That includes textures, models, fonts, title graphics, sounds and the original data tables used by the engine.

The install ZIP contains the executable, launcher icon, instructions and license notices. ROMs, converted assets and caches are not included. The executable still contains compiled engine code; keeping assets out does not make that code independently authored.

## Original data inside engine source

Some original game content appears in the decompiled source as strings and initialized arrays. Simply leaving out separate asset files would still compile that data into the executable.

The build replaces those definitions with empty writable storage. At startup, `native_rom_data_init` fills it from the ROM-derived cartridge file. Layout metadata describes the locations, field widths, byte order and pointer mappings needed to do this.

The recorded development audit covers 1,262 named definitions and 216 compiler-generated data ranges. These include weapon and skeleton definitions, animation and AI tables, menu text, colours, resource names, default records and the attract-demo selection table.

## What the build checks

The audit checks that imported definitions are empty in the compiled output and that the runtime loader reconstructs their values and pointer targets correctly. It also checks anonymous compiler data, including strings that could otherwise be hidden by linker merging.

Some initialized data is needed for the implementation itself: code constants, runtime state, custom menus and compiler jump tables. Those entries have individual reviews with recorded sizes, hashes and source references. Unknown data blocks packaging rather than being accepted automatically.

The detailed report is generated at:

```text
native-engine/build/strict-release-data-audit.json
```

Conversion checks compare models, stage data, audio and rendering caches against PC-generated references. They also cover rejected ROMs, damaged caches, byte-order conversion and interrupted setup.

## Source repository and generated files

This repository includes conversion code and layout metadata, but leaves out the downloaded upstream tree and all generated game files. The setup script fetches the upstream source separately. Builds still need the data-removal and audit steps described above.

Keep `upstream/`, `local/` and `native-engine/build/` out of Git. Older development builds included original initialized data and should not be treated as release packages.

These checks describe the contents of a build. They do not grant rights to inherited engine code or game assets. See the [license guide](../native-engine/distribution/LICENSE.txt).
