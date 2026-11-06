# Gyro look and stereo depth

Enable **Gyro look** in the main menu options, or open **Watch → Game Options → Controls**.

- **Off** keeps the existing controls.
- **Aim only** uses motion while Bond is aiming.
- **Look + aim** uses motion during normal looking and aiming.

**Gyro sensitivity** ranges from 25% to 200%. It is independent of **Look sensitivity**, which controls the sticks. Settings are saved in `native-options.ini`. Existing files without gyro settings start with gyro off and 100% sensitivity.

The input sampler reads angular velocity through libctru, converts raw units using the sensor's coefficient, and integrates elapsed time. It applies a small dead zone, light smoothing and quiet-motion bias correction. Menus, watch pages, scripted control locks, attract demos and application suspension discard motion; long stalls do not accumulate a camera jump. Pitch follows the corrected upright direction; horizontal response is doubled relative to the initial build. Camera updates consume each sample once, scale by zoom FOV, wrap yaw and clamp pitch. Only the controlled player receives motion.

Stereo uses an off-axis eye translation before perspective division for world geometry and first-person weapons. Disparity follows inverse camera distance, independently of the depth-buffer near/far range; world convergence is 600 game units. World geometry retains the full off-axis projection: nearby surfaces are no longer clamped onto one stereo plane. First-person weapons and UI retain the six-pixel per-eye bound. The slider controls eye separation. Depth, perspective W, UVs, gameplay camera projection and the existing menu/watch depth are preserved. The implementation still renders two eyes; it adds shader arithmetic rather than extra scene passes.

Hardware testing is required for gyro axis direction, drift, feel and stereo comfort. Host checks cover sensor units, 30/60Hz integration, input gating, sensitivity independence, persistence and projection bounds. Shader compilation does not replace a console test.

SDK references: [libctru HID API](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/services/hid.h), [HID service implementation](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/hid.c).

**Touch sensitivity** is under **Game Options → Controls** and in the main menu options. It ranges from 25% to 200%, defaults to 100%, and is saved between launches. In the watch, swipe a setting left/right to change it, swipe outside settings rows to switch pages, and swipe vertically to browse weapons or scroll briefing text. Tap the page title or hold a stationary touch to go back. The existing watch page tabs remain touch targets.

**Flick stick** is under **Game Options → Controls** and in the main menu options. It defaults to off and is saved between launches. With **1.5 Vesper**, the C-stick chooses a horizontal turn direction: right turns 90°, down turns 180°, and moving around the edge rotates the view by the same angle. The initial turn takes 100ms; holding the stick still does not keep turning. Return to centre before another flick. Use **Gyro look → Look + aim** or touch dragging for vertical look. Flick turns have fixed angles, independent of stick/gyro sensitivity and zoom. Menus, cutscenes, control locks and suspension discard queued turns. Other control styles retain their existing mapping.
