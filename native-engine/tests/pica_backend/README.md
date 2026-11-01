Run `python3 native-engine/tools/check_pica_backend.py` from the project root.
Set EOG_PICA_SANITIZE=1 and ASAN_OPTIONS=detect_leaks=0 for the sandbox's
ASan/UBSan run (LeakSanitizer is unavailable under ptrace).

The frozen reference is the prior PICA backend from the preserved mounted
checkout, before descriptor caching and deferred draws. Both old and current
code use the installed real Citro3D types and inline TEV builders. Hardware
entry points and allocation/clock functions are recorded/stubbed. Each backend
runs independently against the same fixtures; reference records are expanded
per vertex so batching cannot hide draw-order or GPU-state changes.

Vertex positions, colours and ALL uploaded UV/fog/LOD coordinates must match
bit-for-bit. There is no host shader-arithmetic substitution: the original
pass-through texture-coordinate shader and CPU conversion path are restored.
The previous shader-math emulation missed a real console rendering regression
and is removed. These checks still do not emulate PICA rasterization, texture
sampling or real chip output; console verification remains required.
