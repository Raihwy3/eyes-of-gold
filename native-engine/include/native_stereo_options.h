#pragma once
/* Ghost transform adapted from oneup03/rt64-3ds (MIT).
 * Copyright (c) 2024 RT64 Contributors; see vendor/rt64-stereo/LICENSE. */
#include <algorithm>
namespace native_stereo_options {
inline float convergence(int percent){return 3000.f*std::clamp(percent,10,200)*.01f;}
/* 50 is the LCD plane; 75 preserves the launcher's previous UI offset. */
inline float hud_pixels(int depth){return (std::clamp(depth,0,100)-50)*.16f;}
struct Ghost {float a,b;bool active;};
/* RT64 ghost-reduction affine map, applied once to each finished eye. */
inline Ghost ghost(int contrast,int black_floor){
 float c=std::clamp(contrast,50,100)*.01f,f=std::clamp(black_floor,0,20)*.01f;
 return {c*(1-f),(.5f-.5f*c)*(1-f)+f,contrast<100 || black_floor>0};
}
}
