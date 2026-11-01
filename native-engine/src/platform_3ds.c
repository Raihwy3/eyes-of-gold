/* Platform UI for executing real engine validation routines on the console. */
#include "engine_probe.h"
#include "native_build.h"
#include "native_cart.h"
#include "native_ram.h"
#include <3ds.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NATIVE_GRAPHICS
#include "native_graphics.h"
#include "native_motion.h"
#include "native_room.h"
#endif
const char *sysResolvePath(const char *path) {
  static char result[512];
  if (path[0] == '$' && (path[1] == 'S' || path[1] == 'E') && path[2] == '/')
    path += 3;
  snprintf(result, sizeof result, "sdmc:/3ds/eyes-of-gold-native/%s", path);
  return result;
}
static PrintConsole top, bottom;
static FILE *log_file;
static unsigned failures;
#ifdef NATIVE_GRAPHICS
static bool console_ready;
#endif
void osSyncPrintf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  if (log_file) {
    vfprintf(log_file, fmt, args);
    fflush(log_file);
  }
  va_end(args);
}
void sysLogPrintf(int level, const char *fmt, ...) {
  (void)level;
  va_list args;
  va_start(args, fmt);
  if (log_file) {
    vfprintf(log_file, fmt, args);
    fputc('\n', log_file);
    fflush(log_file);
  }
  va_end(args);
}
uint64_t sysGetMicroseconds(void) {
  uint64_t ticks = svcGetSystemTick();
  return ticks / SYSCLOCK_ARM11 * 1000000u +
         ticks % SYSCLOCK_ARM11 * 1000000u / SYSCLOCK_ARM11;
}
void sysSleep(uint32_t micros) { svcSleepThread((int64_t)micros * 1000); }
uintptr_t sysImageBase(void) {
  extern char _start[];
  return (uintptr_t)_start;
}
void sysFatalError(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  if (log_file) {
    va_list copy;
    va_copy(copy, args);
    fputs("FATAL: ", log_file);
    vfprintf(log_file, fmt, copy);
    fputc('\n', log_file);
    fflush(log_file);
    va_end(copy);
  }
#ifdef NATIVE_GRAPHICS
  if (console_ready) {
    consoleSelect(&bottom);
    consoleClear();
    printf("Native graphics stopped\nBuild: " NATIVE_BUILD_ID "\n\n");
    vfprintf(stderr, fmt, args);
    va_end(args);
    printf("\n\nSTART: exit\nDetails: native-engine.log\n");
    // The render frame can be half built here. Keep the single-buffer bottom
    // console visible without presenting or deleting any GPU render targets.
    bool released = false;
    while (aptMainLoop()) {
      hidScanInput();
      if (!(hidKeysHeld() & KEY_START))
        released = true;
      if (released && (hidKeysDown() & KEY_START))
        break;
      gfxFlushBuffers();
      gspWaitForVBlank();
    }
    exit(1);
  }
#endif
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(1);
}
static void report(const char *name, int success, unsigned value) {
  if (!strcmp(name, "Original pathfinder progress")) {
    if (!aptMainLoop())
      return;
    return;
  }
  consoleSelect(&top);
  printf("%s %s\n  %u\n", success ? "PASS" : "FAIL", name, value);
  if (!success)
    ++failures;
  if (log_file) {
    fprintf(log_file, "%s %s: %u\n", success ? "PASS" : "FAIL", name, value);
    fflush(log_file);
  }
  gfxFlushBuffers();
  gfxSwapBuffers();
  gspWaitForVBlank();
}
int main(void) {
  gfxInitDefault();
  consoleInit(GFX_TOP, &top);
  consoleInit(GFX_BOTTOM, &bottom);
#ifdef NATIVE_GRAPHICS
  console_ready = true;
#endif
  consoleSelect(&bottom);
  printf("Eyes of Gold - Native engine\nBuild: " NATIVE_BUILD_ID
         "\n\nOriginal decompilation validation.\nThis is NOT a playable game "
         "build.\n\nNo recreation gameplay is linked.\n\nSTART: exit\n");
  consoleSelect(&top);
  printf("ORIGINAL ENGINE / ARMv6K\n\n");
  log_file = fopen("sdmc:/3ds/eyes-of-gold-native/native-engine.log", "w");
  if (log_file)
    fprintf(log_file, "Native diagnostic build: " NATIVE_BUILD_ID "\n");
  int passed =
      engine_probe("sdmc:/3ds/eyes-of-gold-native/native-probe.dat", report);
  printf("\n%s\n", passed && !failures ? "All original-code checks passed."
                                       : "Validation failed; see log.");
  if (log_file) {
    fprintf(log_file, "Final: %s\n", passed && !failures ? "PASS" : "FAIL");
    fflush(log_file);
#ifndef NATIVE_GRAPHICS
    fclose(log_file);
    log_file = NULL;
#endif
  }
#ifdef NATIVE_GRAPHICS
  if (passed && !failures) {
    consoleSelect(&bottom);
    consoleClear();
    printf("Eyes of Gold: Native Dam walk\nBuild: " NATIVE_BUILD_ID
           "\n\nOriginal rooms / gait / collision\n\nCircle Pad: move\nC-stick: "
           "look; L/R: turn\nD-pad up/down: look\nY: reset to starting "
           "position\nX: Dam; A: menu; B: triangles\nSTART: exit\n\nNo guards, "
           "props or mission yet.\nLoading original Dam...\n");
    gfxFlushBuffers();
    if (native_scene_load("sdmc:/3ds/eyes-of-gold-native/native-menu.rle") &&
        native_room_load("sdmc:/3ds/eyes-of-gold-native/native-room.dat") &&
        native_world_load("sdmc:/3ds/eyes-of-gold-native/native-world.dat") &&
        native_motion_load("sdmc:/3ds/eyes-of-gold-native/native-motion.dat")) {
      gfxFlushBuffers();
      native_graphics_init();
      unsigned frame = 0, mode = 2;
      float yaw = 1.5707963f, pitch = 0.f;
      uint64_t last_tick = sysGetMicroseconds();
      if (log_file) {
        fputs("Original Fast3D -> PICA initialized\n", log_file);
        fflush(log_file);
      }
      while (aptMainLoop()) {
        hidScanInput();
        unsigned keys = hidKeysDown();
        if (keys & KEY_START)
          break;
        if (keys & KEY_A)
          mode = 0;
        if (keys & KEY_B)
          mode = 1;
        if (keys & KEY_X)
          mode = 2;
        if (keys & KEY_Y) {
          native_world_reset();
          yaw = 1.5707963f;
          pitch = 0.f;
        }
        uint64_t now = sysGetMicroseconds();
        float dt = (now - last_tick) * .000001f;
        last_tick = now;
        if (dt > .05f)
          dt = .05f;
        if (mode == 2) {
          circlePosition pad, stick;
          hidCircleRead(&pad);
          hidCstickRead(&stick);
          if (stick.dx > 12 || stick.dx < -12)
            yaw -= stick.dx * .012f * dt;
          if (stick.dy > 12 || stick.dy < -12)
            pitch += stick.dy * .010f * dt;
          unsigned held = hidKeysHeld();
          if (held & KEY_L)
            yaw += 1.5f * dt;
          if (held & KEY_R)
            yaw -= 1.5f * dt;
          if (held & KEY_DUP)
            pitch += dt;
          if (held & KEY_DDOWN)
            pitch -= dt;
          if (pitch > 1.4f)
            pitch = 1.4f;
          if (pitch < -1.4f)
            pitch = -1.4f;
          float sideways = (pad.dx > 12 || pad.dx < -12) ? pad.dx / 156.f : 0.f;
          float forward = (pad.dy > 12 || pad.dy < -12) ? pad.dy / 156.f : 0.f;
          float magnitude = sqrtf(sideways * sideways + forward * forward);
          if (magnitude > 1.f) {
            sideways /= magnitude;
            forward /= magnitude;
          }
          native_world_update(sideways, forward, yaw, dt);
        }
        if (mode == 2)
          native_world_render(yaw, pitch);
        else
          native_scene_frame(frame, mode);
        ++frame;
        if (log_file && frame % 120 == 0) {
          fprintf(
              log_file,
              "Graphics mode %u: %u frames, %u triangles, %u texture uploads\n",
              mode, native_graphics_frames(), native_graphics_triangles(),
              native_graphics_uploads());
          fflush(log_file);
        }
      }
      native_graphics_shutdown();
      native_scene_destroy();
      if (log_file) {
        fclose(log_file);
        log_file = NULL;
      }
      native_cart_reset();
      native_ram_destroy();
      gfxExit();
      return 0;
    } else {
      consoleSelect(&bottom);
      printf("ERROR: menu/room/world/motion data invalid\n");
    }
  }
#endif
  while (aptMainLoop()) {
    hidScanInput();
    if (hidKeysDown() & KEY_START)
      break;
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
  }
  if (log_file) {
    fclose(log_file);
    log_file = NULL;
  }
  native_cart_reset();
  native_ram_destroy();
  gfxExit();
  return 0;
}
