#pragma once
/* Shared by world projection, adaptive convergence, sky and aim-depth lookup.
 * Separation is total background disparity as a fraction of LCD width. */
namespace native_stereo_profile {
constexpr float separation=.03f;
constexpr float manual_convergence=3000.f;
}
