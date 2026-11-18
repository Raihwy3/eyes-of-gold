# Weapon stereo and material planning

First-person weapon draws now use separate left and right projection matrices.
Disparity varies with clip-space depth; this is more than a fixed offset of the
whole weapon. The slider controls the effect. World draws use
their own depth-dependent eye matrices too, on both CPU clip and GPU model paths.
The world profile places distant scenery behind the LCD and nearby geometry in
front, while UI and weapons keep their separate profiles. Draws already ran for
both eyes, so this change does not add another geometry traversal. Appearance
and comfort still need console testing.

CPU-side planning and submission changes accompany this:

- Find short compatible triangle runs with exponential probes before bisecting.
- Reuse the fog compatibility result between the normal and fallback colour paths.
- Ignore redundant clip-shader, UI-rectangle and weapon-scope resets. Real scope
  transitions still flush, preserving ordered geometry and material changes.

These changes do not remove geometry or alter guard updates. A host benchmark of
short compatible runs improved, but that does not establish a console FPS gain.

Run the standalone checks with:

```
python3 native-engine/tools/check_weapon_stereo.py
```

These compare 4,000 material-prefix fixtures against the previous algorithm and
check 3,030 weapon and world projection cases each. Backend reference comparisons
also passed, including scope transitions and stereo submissions at both render scales.
The initial isolated backend checks used the available Perfect Dark Fast3D
headers and combiner implementation while the GoldenEye checkout was absent.
After restoring the pinned GoldenEye checkout, the backend reference comparisons
also passed against GoldenEye's own Fast3D headers and combiner implementation.
Host checks do not replace hardware testing.
