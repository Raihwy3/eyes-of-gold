# Gyro look and stereo depth

Enable **Gyro look** in the main menu options, or open **Watch → Game Options → Controls**.

- **Off** keeps the existing controls.
- **Aim only** uses motion while Bond is aiming.
- **Look + aim** uses motion during normal looking and aiming.

**Look, Gyro and Touch sensitivity** all use 10–400%, in 10% steps, with a faster 100% default. Existing saved speeds are preserved: the previous 100% appears as 40%. Set it to 100% to use the faster baseline. The config file retains its legacy gain units for compatibility.

**Gyro sensitivity** ranges from 10% to 200%. It is independent of **Look sensitivity**, which controls the sticks. Settings are saved in `native-options.ini`. Existing files without gyro settings start with gyro off and 100% sensitivity.

The input sampler converts gyro readings to degrees per second and accelerometer readings to g before passing them to [GamepadMotionHelpers](https://github.com/JibbSmart/GamepadMotionHelpers). At every setting, equal pitch and yaw rotations have equal angular gain. The new 100% gyro baseline is 2.5 degrees of camera rotation per degree of device rotation before zoom scaling; horizontal motion has no separate multiplier. Zoom applies the same FOV adjustment to both axes. Sticks retain their separate look sensitivity.

**Gyro space** offers Local, World and Player. Local uses pitch plus exactly one **Local turn axis**, Yaw or Roll. World projects rotation against gravity; Player uses gravity-relative turning with local pitch. These gravity-relative modes intentionally use different sensor axes as the handheld tilts, rather than adding local yaw and roll together. The axis selector displays Gravity in these modes. Local/Yaw remains the default.

Choose **Calibrate gyro**, then leave the console still for three seconds. The menu shows the percentage while sampling and Calibrated when finished. Movement restarts the sampling period. Stillness uses changes in the sensor readings, so a stable raw gyro offset or accelerometer scale difference does not prevent calibration. After twelve seconds without a stable window, Try again appears and the previous calibration is kept. Calibration also works with gyro look off. Calibration offsets last for the session; repeat after restarting if needed. Hybrid automatic calibration runs only while motion look is disengaged, so deliberate slow aiming is not learned as drift. Menus, watch pages, scripted locks, attract demos and suspension discard motion, and long stalls cannot accumulate a camera jump.

**Watch → Game Options → Accessibility** contains Aim lock (default Off), Stamina sway (default Off) and Steady scope (default On). Aim lock centres the reticle and routes aiming into the camera while aiming; Off keeps free aiming. Stamina sway controls extra fatigue-related breathing sway rather than recoil or all movement bob. Steady scope fades head-animation drift as FOV narrows from 50 to 20 degrees. Demo playback retains its original camera behaviour. These choices are also available in main-menu options and are saved.

**Watch → Game Options → Controls → Aim movement** switches between Free (the current default) and Classic for Vesper/Jinx. Classic uses Circle Pad left/right to lean in place through the original engine lean system, blocks walking and strafing, and uses down/up to crouch/stand. With scoped weapons, up/down adjusts zoom instead, as in the original controls. Releasing aim restores normal movement and keeps the selected crouch height. Tank controls and the original control styles retain their existing behaviour. The choice is also available in main-menu options and is saved.

With Vesper, hold aim and use D-pad up/down to zoom the sniper in/out. With Jinx, hold aim and use X/B; those buttons adjust zoom instead of vertical look while held.

Stereo uses an off-axis eye translation before perspective division for world geometry and first-person weapons. Disparity follows inverse camera distance, independently of the depth-buffer near/far range; world convergence is 600 game units. World geometry retains the full off-axis projection: nearby surfaces are no longer clamped onto one stereo plane. First-person weapons and UI retain the six-pixel per-eye bound. The slider controls eye separation. Depth, perspective W, UVs, gameplay camera projection and the existing menu/watch depth are preserved. The implementation still renders two eyes; it adds shader arithmetic rather than extra scene passes.

Hardware testing is required for gyro axis direction, gravity-space feel, drift and stereo comfort. Host checks cover sensor units, 30/60Hz integration, input gating, sensitivity independence, persistence and projection bounds. Shader compilation does not replace a console test.

SDK references: [libctru HID API](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/services/hid.h), [HID service implementation](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/hid.c).

**Touch sensitivity** is under **Game Options → Controls** and in the main menu options. It ranges from 10% to 200%, defaults to 100%, and is saved between launches. In the watch, swipe a setting left/right to change it, swipe outside settings rows to switch pages, and swipe vertically to browse weapons or scroll briefing text. Tap the page title or hold a stationary touch to go back. The existing watch page tabs remain touch targets.

**Flick stick (exp)** is under **Game Options → Controls** and in the main menu options. It defaults to off and is saved between launches. With **1.5 Vesper**, the C-stick chooses a horizontal turn direction: right turns 90°, down turns 180°, and moving around the edge rotates the view by the same angle. JoyShockMapper’s flick timing and magnitude-dependent rotation smoothing are adapted directly from revision `980dc52c09fbdfb3378b04a8a1895ea3157c29e1` (MIT). The initial turn eases out over 100ms; holding the stick still does not keep turning. Slow circular movement also works at lighter pressure. Return to centre briefly before another flick; a single neutral dropout does not re-arm it. Use **Gyro look → Look + aim** or touch dragging for vertical look. Flick turns have fixed angles, independent of stick/gyro sensitivity and zoom. Menus, cutscenes, control locks and suspension discard queued turns. Other control styles retain their existing mapping.

While holding aim with Aim Lock off, gyro and touch dragging move the free reticle and weapon aim within the original aiming area. The camera moves only for motion beyond that area. Releasing aim returns gyro to normal camera look when Look + aim is selected. Touch aim works with gyro disabled and retains the reticle position when dragging stops. Aim Lock deliberately keeps it centred and uses touch for camera look. Gyro aim retains its own sensitivity and zoom scaling, independent of stick sensitivity.


### Adaptive stereo

The convergence controller and 9x5 patch statistic now come from oneup03's
rt64-3ds at `0d783be0c9f51fa7edc696ac33a0992b5f021ecb`. Their MIT license and
source fingerprints are in `native-engine/vendor/rt64-stereo/`.

The earlier sparse approximation has been replaced. This renderer now queues a
GPU copy of the completed world depth before first-person weapons draw, consumes
it after the next normal frame wait, and samples an 8-pixel grid over the visible
viewport. Patch percentiles, the second-nearest patch, nine-frame history,
approach-only recent-median cut detection, initialisation at the solved target
and reciprocal convergence easing follow the reference. The reference's zero
pop-out comfort target is used; its EMA rates are normalised to elapsed time.
The lower quarter is excluded from the near statistic as in rt64-3ds.

The reticle uses the reference's thirteen candidate positions and 5x5 sample
patches, selects the nearest consistent finite depth, smooths depth rather than
pixel displacement, and holds it across sky, missing depth and gaps. Reticle
position comes from the actual draw in LCD pixels, including its viewport. The
previous frame's separation and convergence correct the left-eye sample. The
reference's asymmetric NDC limits are applied to the reticle; world vertices
retain off-axis depth without individual clamping.

Projection coefficients are captured from the display list rather than from a
mutable game global. GPU depth scale/bias, rotation, AA scale, padded target
dimensions and the original perspective coefficients are retained with the
snapshot. Combined room-camera matrices do not replace the canonical projection.
Watch pauses and render-target changes discard unavailable samples while holding
world convergence, avoiding a depth jump back to the manual value. The tagged
sky gets infinity disparity and backdrop-only cover scaling. The
watch and other HUD elements retain their existing separate placement.

The depth copy needs at most 1 MiB of linear memory and an additional queued GPU
transfer, not an additional GPU wait. Allocation failure leaves fixed stereo and
logs the failure; it does not substitute contaminated weapon depth. Native and
supersampled targets are supported. Multiplayer preview retains fixed
convergence. No frame interpolation, head tracking or RT64 runtime replacement
is included. Hardware comfort and performance remain unverified.

References:
- https://github.com/oneup03/3D-Skills/tree/main/dynamic3d
- https://github.com/oneup03/rt64-3ds

The adapter uses JSM’s default unsnapped angles, 100ms flick time, zero timing exponent, and 0.08/0.16-radian smoothing thresholds with a roughly 64ms history. Mouse calibration is replaced by direct camera degrees. C-stick pressure thresholds, neutral debounce and smoothing-tail draining are 3DS-specific; desktop device mapping and mouse output are not included.

Advanced → Experimental → 3D strength scales the physical slider from 0% to 200% in 25% steps, defaults to 100%, and is saved. Look, gyro and touch sensitivity now extend to 400%; existing saved values retain the same speed.

World-depth tuning: the shared clip-space separation is now 0.03 (12 pixels total background disparity on a 400-pixel eye image at full slider/100% strength). The manual convergence ceiling is 3000 camera-W units; the reference controller still pulls it inward for nearby geometry. Projection, depth-aware reticle sampling and sky coverage use the same separation constant. Weapon and watch placement retain their separate profiles. This tuning requires console evaluation.
