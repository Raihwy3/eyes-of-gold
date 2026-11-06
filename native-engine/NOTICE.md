# Native code license

The [MIT license](LICENSE) covers the original Eyes of Gold code and changes, including the 3DS platform layer, rendering additions, controls, menus and tools.

It does not cover inherited GoldenEye decompilation code, game assets or third-party code. A patched copy of an upstream file can contain both inherited code and original changes; its location in this project does not put the whole file under MIT.

The [license guide](distribution/LICENSE.txt) explains the separate terms. [SOURCES.txt](distribution/licenses/SOURCES.txt) lists component credits and source locations.

Motion controls use GamepadMotionHelpers by Julian “Jibb” Smart (MIT).
The source and license are in vendor/GamepadMotionHelpers; the packaged notice
is distribution/licenses/GamepadMotionHelpers.txt. See docs/gyro-stereo.md in
the repository for gyro spaces, calibration and accessibility behaviour.

Stereo convergence and sampling include code adapted from oneup03/rt64-3ds
(commit 0d783be0c9f51fa7edc696ac33a0992b5f021ecb), MIT, Copyright (c) 2024
RT64 Contributors. See licenses/rt64-stereo.txt and vendor/rt64-stereo/source.json.

Flick-stick timing and rotation smoothing use JoyShockMapper by Julian "Jibb" Smart and contributors (MIT).
Source: https://github.com/JibbSmart/JoyShockMapper
Revision: 980dc52c09fbdfb3378b04a8a1895ea3157c29e1
Adapted functions: handleFlickStick and GetSmoothedStickRotation in JoyShockMapper/src/main.cpp.
License: JoyShockMapper.txt. The 3DS adapter uses camera degrees, pressure-nub thresholds and debounced release.

Thanks to IDrinkLava on GBAtemp for gyro feedback and reference suggestions,
and OneUp on GBAtemp for the dynamic3d and rt64-3ds stereo references.
See the root CREDITS.md for acknowledgements.
