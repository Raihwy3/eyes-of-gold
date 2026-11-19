/* Original MIT renderer reference, pinned in vendor/conker-reticle/source.json. */
#include <algorithm>
#include <cmath>
#include <cstdint>
static void conker_reference(int style,uint32_t *pixels){
 const uint8_t cr=255,cg=255,cb=255;
 constexpr int kN=64;
 auto put_px8=[&](int x,int y,uint32_t value){pixels[y*64+x]=value;};
            auto sd_box = [](float px, float py, float bx, float by) -> float {
                const float dx = std::fabs(px) - bx, dy = std::fabs(py) - by;
                const float ox = std::max(dx, 0.0f), oy = std::max(dy, 0.0f);
                return std::hypot(ox, oy) + std::min(std::max(dx, dy), 0.0f);
            };
            auto smooth = [](float edge0, float edge1, float x) -> float {
                const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
                return t * t * (3.0f - 2.0f * t);
            };
            for (int y = 0; y < kN; y++) {
                for (int x = 0; x < kN; x++) {
                    const float px = (float)x - 31.5f;
                    const float py = (float)y - 31.5f;
                    const float r = std::hypot(px, py);
                    float d = 100.0f;
                    if (style == 1) {
                        d = r - 5.2f;
                    } else if (style == 2 || style == 3 || style == 6) {
                        const float dl = sd_box(px + 15.5f, py, 6.8f, 1.9f);
                        const float dr = sd_box(px - 15.5f, py, 6.8f, 1.9f);
                        const float db = sd_box(px, py - 15.5f, 1.9f, 6.8f);
                        d = std::min(std::min(dl, dr), db);
                        if (style != 6) {
                            const float dt = sd_box(px, py + 15.5f, 1.9f, 6.8f);
                            d = std::min(d, dt);
                        }
                        if (style == 3) {
                            d = std::min(d, r - 3.3f);
                        }
                    } else if (style == 4 || style == 5) {
                        d = std::fabs(r - 17.5f) - 2.1f;
                        if (style == 5) {
                            d = std::min(d, r - 3.3f);
                        }
                    } else if (style == 7) {
                        const float man = (std::fabs(px) + std::fabs(py)) * 0.70710678f;
                        d = std::fabs(man - 13.5f) - 2.1f;
                    }
                    const float fill_a = 1.0f - smooth(-1.2f, 1.2f, d);
                    const float out_a  = (1.0f - smooth(-1.2f, 1.2f, d - 2.6f)) * 0.78f;
                    const float comb_a = fill_a + out_a * (1.0f - fill_a);
                    uint8_t r8 = 0, g8 = 0, b8 = 0, a8 = 0;
                    if (comb_a > 0.002f) {
                        const float w = fill_a / comb_a;
                        r8 = (uint8_t)std::lround((float)cr * w);
                        g8 = (uint8_t)std::lround((float)cg * w);
                        b8 = (uint8_t)std::lround((float)cb * w);
                        a8 = (uint8_t)std::lround(comb_a * 255.0f);
                    }
                    put_px8(x, y, ((uint32_t)r8 << 24) | ((uint32_t)g8 << 16) | ((uint32_t)b8 << 8) | a8);
                }
            }

}
