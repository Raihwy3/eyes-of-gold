// Adapted from oneup03/rt64-3ds at 0d783be0c9f51fa7edc696ac33a0992b5f021ecb.
// Copyright (c) 2024 RT64 Contributors. MIT; see LICENSE.
// Unmodified reference algorithms for host comparison; standalone declarations only.
#pragma once
#include <algorithm>
#include <cmath>
namespace eog_stereo_reference_test {
struct AutoConvTuning {
    float region_bottom = 0.75f;    // the near statistic samples the frame down to here (fraction of its height)
    int history = 9;                // frames in the median filter (1..9)
    float approach = 0.25f;         // per-frame pull toward a nearer depth (the retreat is slower)
    float retreat = 0.05f;          // ... and toward a farther one
    float cut_ratio = 2.5f;         // this much nearer for 2 frames is a camera cut: jump there
    float ease_in = 0.14f, ease_out = 0.06f;   // convergence easing toward the target, nearer / farther
    float near_clamp = 4.0f;        // convergence never comes nearer than this (world units)
};
struct AutoConvergence {
    static constexpr int kHistory = 9;
    float history[kHistory] = {};
    int count = 0, cursor = 0;
    float z_ema = -1.0f;
    float inv_conv = -1.0f;
    int cut_frames = 0;
    float last_conv = -1.0f;
    // Returns the convergence to apply, world units.
    float update(float nearest_z, float manual_conv, float sep, int comfort_thousandths, const AutoConvTuning& tune);
    void reset() { count = cursor = cut_frames = 0; z_ema = inv_conv = last_conv = -1.0f; }
};
inline float AutoConvergence::update(float nearest_z, float manual_conv, float sep, int comfort_thousandths, const AutoConvTuning& tune) {
    if (sep <= 0.0f || manual_conv <= 0.0f) return manual_conv;
    // No near depth this frame (a sky, a menu): keep the last convergence.
    // Returning the manual value instead made a sky with a few birds in it
    // flap between the two, frame by frame, as the birds came and went.
    if (nearest_z <= 0.0f) return last_conv > 0.0f ? std::min(last_conv, manual_conv) : manual_conv;

    const int len = std::max(1, std::min(kHistory, tune.history));
    history[cursor] = nearest_z;
    cursor = (cursor + 1) % kHistory;
    count = count < len ? count + 1 : len;
    float sorted[kHistory];
    for (int i = 0; i < count; i++) sorted[i] = history[(cursor + kHistory - 1 - i) % kHistory];   // the newest `count`
    std::sort(sorted, sorted + count);
    const float z_median = sorted[count / 2];

    if (z_ema <= 0.0f) {
        z_ema = z_median;
    }
    else {
        const float rel = std::fabs(z_median - z_ema) / z_ema;
        // A camera cut stays changed; a small object flickering in and out of
        // a patch does not, so the snap waits for two frames of it.
        const float approach = nearest_z < z_ema ? z_ema / nearest_z : 1.0f;
        cut_frames = approach > tune.cut_ratio ? cut_frames + 1 : 0;
        if (cut_frames >= 2) {
            float recent[3];
            for (int i = 0; i < 3; i++) recent[i] = history[(cursor + kHistory - 1 - i) % kHistory];
            std::sort(recent, recent + 3);
            z_ema = recent[1];
            inv_conv = -1.0f;
            cut_frames = 0;
        }
        else if (rel > 0.02f) {
            // Quick to pull in as something approaches, slow to let go.
            const float alpha = z_median < z_ema ? tune.approach : tune.retreat;
            z_ema += (z_median - z_ema) * alpha;
        }
    }

    float target = (float)std::max(-50, std::min(60, comfort_thousandths)) * 0.001f;
    const float at_manual = sep * 0.5f * (manual_conv / z_ema - 1.0f);
    float conv = manual_conv;
    if (at_manual > target) {
        conv = std::min(z_ema * (1.0f + 2.0f * target / sep), manual_conv);
    }
    conv = std::max(conv, z_ema * 0.25f);
    conv = std::max(conv, tune.near_clamp);

    const float target_inv = 1.0f / conv;
    if (inv_conv <= 0.0f) inv_conv = target_inv;
    else inv_conv += (target_inv - inv_conv) * (target_inv > inv_conv ? tune.ease_in : tune.ease_out);
    last_conv = std::min(1.0f / inv_conv, manual_conv);
    return last_conv;
}

inline float near_depth_statistic(const float* grid, int cols, int rows, float region_bottom) {
    constexpr int kCols = 9, kRows = 5;
    const float x0 = cols * 0.05f, x1 = cols * 0.95f, y0 = rows * 0.05f, y1 = rows * std::max(0.3f, std::min(1.0f, region_bottom));
    float nears[kCols * kRows];
    int n = 0;
    float samples[64];
    for (int py = 0; py < kRows; py++) {
        const int ya = (int)(y0 + (y1 - y0) * py / kRows), yb = (int)(y0 + (y1 - y0) * (py + 1) / kRows);
        for (int px = 0; px < kCols; px++) {
            const int xa = (int)(x0 + (x1 - x0) * px / kCols), xb = (int)(x0 + (x1 - x0) * (px + 1) / kCols);
            int m = 0;
            for (int y = ya; y < yb && y < rows; y++) {
                for (int x = xa; x < xb && x < cols; x++) {
                    const float z = grid[y * cols + x];
                    if (z > 0.0f && z < 1.0f && m < 64) samples[m++] = z;
                }
            }
            if (m < 4) continue;
            // A low percentile, so one stray texel cannot pull the value.
            std::nth_element(samples, samples + m / 4, samples + m);
            nears[n++] = samples[m / 4];
        }
    }
    // Fewer than 3 patches with depth is no scene to converge on: a sky with
    // a bird or two in it. (A 3DS patch is about 16 samples of 8x8 pixels,
    // so a small object fills one easily; the desktop's are finer.)
    if (n < 3) return -1.0f;
    // The second nearest patch, so one small object is not the whole story.
    const int k = n >= 4 ? 1 : 0;
    std::nth_element(nears, nears + k, nears + n);
    return nears[k];
}

}
