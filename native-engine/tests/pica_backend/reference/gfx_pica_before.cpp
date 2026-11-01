/* First PICA backend for the original Fast3D interpreter. No recreation code.
 * Unsupported material/framebuffer features fail explicitly during bring-up.
 */
#include "gfx_cc.h"
#include "gfx_rendering_api.h"
#include "gfx_window_manager_api.h"
#include "native_clip_shbin.h"
#include "native_display.h"
#include "native_graphics.h"
#include "pica_depth.h"
#include "pica_fog.h"
#include "pica_inputs.h"
#include "pica_batch.h"
#include "pica_alpha_product.h"
#include "pica_tev_cache.h"
#include "pica_texcoord.h"
#include "pica_noise.h"
#include "native_cart.h"
#include "pica_combiner.h"
#include <3ds.h>
#include <algorithm>
#include <citro3d.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" void sysFatalError(const char *, ...) __attribute__((noreturn));
extern "C" void native_graphics_destroy_interpreter(void);
extern "C" void sysLogPrintf(int, const char *, ...);
struct ShaderProgram {
  uint64_t id0;
  uint32_t id1;
  CCFeatures cc;
  bool colors_logged;
};
struct Texture {
  C3D_Tex gpu;
  unsigned width, height, levels;
  bool ready, linear, requested_mips;
  unsigned cms, cmt;
};
struct Vertex {
  float pos[4], color[4], uv[3][2];
};
static C3D_RenderTarget *target;
static DVLB_s *shader;
static shaderProgram_s program;
static std::vector<ShaderProgram *> shaders;
static std::vector<Texture *> textures(1, nullptr);
static std::vector<C3D_Tex> retired;
static ShaderProgram *active;
static unsigned upload_tile;
static unsigned selected[2], next_fb = 1, frame_count, triangle_count,
                             upload_count;
static bool in_frame;
static unsigned gpu_phase;
extern "C" unsigned native_graphics_phase(void) {return __atomic_load_n(&gpu_phase,__ATOMIC_ACQUIRE); }
static unsigned bright_vertices,clip_vertices,draw_calls,near_vertices,far_vertices,behind_vertices;
static int last_viewport[4],last_scissor[4];
static bool last_depth,last_blend;

static Vertex *vertices;
static size_t vertex_count;
static constexpr unsigned MAX_VERTICES = 32768;
static FilteringMode filtering = FILTER_NONE;
static MipmapFilteringMode mip_filter = MIPMAP_DISABLED;
static C3D_Tex scalar_ramp;
static C3D_Tex noise_samples[2];
static PicaInputPlan inputs;
static bool tev_valid;
static PicaTevKey tev_key;
static unsigned tev_rgb[2],tev_alpha[2],tev_configs,tev_reuses;
static u64 gpu_wait_start,cpu_frame_start;
static double gpu_wait_ms;
static u64 draw_ticks,upload_ticks,planning_ticks;
static unsigned draw_depth;
struct DrawCost {
  u64 start;bool outer;
  DrawCost():start(svcGetSystemTick()),outer(draw_depth++==0) {}
  ~DrawCost(){--draw_depth;if(outer)draw_ticks+=svcGetSystemTick()-start;}
};
struct UploadCost {
  u64 start=svcGetSystemTick();
  ~UploadCost(){upload_ticks+=svcGetSystemTick()-start;}
};

[[noreturn]] static void unsupported(const char *what) {
  if (active)
    sysLogPrintf(0,
                 "Unsupported PICA material %llx/%x: inputs=%u fog=%u noise=%u "
                 "grayscale=%u blur=%u",
                 (unsigned long long)active->id0, active->id1,
                 active->cc.num_inputs, active->cc.opt_fog,
                 active->cc.opt_noise, active->cc.opt_grayscale,
                 active->cc.opt_blur);
  sysFatalError("Native PICA: unsupported %s", what);
}
static const char *name() { return "PICA200 / native Fast3D"; }
static int max_texture() { return 1024; }
static GfxClipParameters clip() { return {false, false}; }
static void unload(ShaderProgram *) { active = nullptr; }
static void load(ShaderProgram *s) { active = s; }
static ShaderProgram *lookup(uint64_t a, uint32_t b) {
  for (auto *s : shaders)
    if (s->id0 == a && s->id1 == b)
      return s;
  return nullptr;
}
static ShaderProgram *create(uint64_t a, uint32_t b) {
  auto *s = new ShaderProgram{};
  s->id0 = a;
  s->id1 = b;
  gfx_cc_get_features(a, b, &s->cc);
  shaders.push_back(s);
  active = s;
  return s;
}
static void info(ShaderProgram *s, uint8_t *n, bool used[2]) {
  *n = s->cc.num_inputs;
  used[0] = s->cc.used_textures[0];
  used[1] = s->cc.used_textures[1];
}
static void clear_shaders() {
  for (auto *s : shaders)
    delete s;
  shaders.clear();
  active = nullptr;
}
static uint32_t new_texture() {
  textures.push_back(new Texture{});
  return textures.size() - 1;
}
static Texture *texture(int tile) {
  unsigned id = selected[tile];
  if (!id || id >= textures.size() || !textures[id])
    sysFatalError("Native PICA: invalid texture %u", id);
  return textures[id];
}
static void select_texture(int tile, uint32_t id, bool) {
  if (tile < 0 || tile > 1 || id >= textures.size())
    sysFatalError("Native PICA: bad texture binding");
  selected[tile] = id;
  upload_tile = tile;
}
static void dispose(C3D_Tex gpu) {
  if (in_frame)
    retired.push_back(gpu);
  else
    C3D_TexDelete(&gpu);
}
static unsigned power2(unsigned n) {
  unsigned p = 8;
  while (p < n)
    p *= 2;
  return p;
}
static unsigned morton(unsigned x, unsigned y) {
  return (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) |
         ((x & 4) << 2) | ((y & 4) << 3);
}
static void apply_sampler(Texture *tex);
static void init_noise_samples() {
  // Immutable random fields; alpha is evaluated by TEV after texture sampling.
  for(int variant=0;variant<2;++variant) {
    C3D_Tex *tex=&noise_samples[variant];
    if(!C3D_TexInit(tex,256,256,GPU_A8))
      sysFatalError("Native PICA: noise lookup allocation");
    auto *data=static_cast<unsigned char*>(tex->data);
    for(unsigned y=0;y<256;++y) for(unsigned x=0;x<256;++x)
      data[((y/8)*32+x/8)*64+morton(x&7,y&7)]=pica_noise_sample(x,y,variant+2);
    C3D_TexSetWrap(tex,GPU_REPEAT,GPU_REPEAT);
    C3D_TexSetFilter(tex,GPU_NEAREST,GPU_NEAREST);
    C3D_TexFlush(tex);
  }
}
static void upload(const uint8_t *rgba, uint32_t w, uint32_t h, bool mipmaps) {
  UploadCost cost;
  if (!w || !h || w > 1024 || h > 1024)
    sysFatalError("Native PICA: texture size %ux%u", w, h);
  Texture *t = texture(upload_tile);
  if (t->ready)
    dispose(t->gpu);
  unsigned pw = power2(w), ph = power2(h);
  if (!(mipmaps ? C3D_TexInitMipmap(&t->gpu, pw, ph, GPU_RGBA8)
                : C3D_TexInit(&t->gpu, pw, ph, GPU_RGBA8)))
    sysFatalError("Native PICA: texture allocation failed");
  t->levels = t->gpu.maxLevel;
  t->width = w;
  t->height = h;
  t->ready = true;
  auto *dst = static_cast<uint8_t *>(t->gpu.data);
  // Replicate edge pixels into POT padding. Store rows consistently with UVs;
  // GPU_RGBA8 bytes are A,B,G,R, and each 8x8 tile uses Morton ordering.
  for (unsigned y = 0; y < ph; ++y)
    for (unsigned x = 0; x < pw; ++x) {
      unsigned src =
          (std::min<unsigned>(y, h - 1) * w + std::min<unsigned>(x, w - 1)) * 4;
      unsigned out =
          ((y / 8) * (pw / 8) * 64 + (x / 8) * 64 + morton(x & 7, y & 7)) * 4;
      dst[out] = rgba[src + 3];
      dst[out + 1] = rgba[src + 2];
      dst[out + 2] = rgba[src + 1];
      dst[out + 3] = rgba[src];
    }
  if (mipmaps)
    C3D_TexGenerateMipmap(&t->gpu, GPU_TEXFACE_2D);
  apply_sampler(t);
  C3D_TexFlush(&t->gpu);
  ++upload_count;
}
static void apply_sampler(Texture *tex) {
  if (!tex->ready)
    return;
  auto wrap = [](unsigned value) {
    return (value & 2)   ? GPU_CLAMP_TO_EDGE
           : (value & 1) ? GPU_MIRRORED_REPEAT
                         : GPU_REPEAT;
  };
  C3D_TexSetFilter(&tex->gpu, tex->linear ? GPU_LINEAR : GPU_NEAREST,
                   tex->linear ? GPU_LINEAR : GPU_NEAREST);
  tex->gpu.maxLevel =
      tex->requested_mips && mip_filter != MIPMAP_DISABLED ? tex->levels : 0;
  C3D_TexSetFilterMipmap(&tex->gpu, mip_filter == MIPMAP_LINEAR ? GPU_LINEAR
                                                                : GPU_NEAREST);
  C3D_TexSetWrap(&tex->gpu, wrap(tex->cms), wrap(tex->cmt));
}
static void sampler(int tile, bool linear, uint32_t s, uint32_t t, bool mip) {
  Texture *tex = texture(tile);
  tex->linear = linear;
  tex->cms = s;
  tex->cmt = t;
  tex->requested_mips = mip;
  apply_sampler(tex);
}

static void depth(bool test, bool write, bool compare, bool prim,
                  uint16_t mode) {
  last_depth=test;
  PicaDepthPlan plan;
  if (!pica_plan_depth(test, write, compare, prim, mode, &plan))
    unsupported("depth mode");
  C3D_DepthTest(plan.test, plan.compare ? GPU_GEQUAL : GPU_ALWAYS,
                plan.write ? GPU_WRITE_ALL : GPU_WRITE_COLOR);
  C3D_DepthMap(true, -1.f, plan.bias);
}

static void depth_range(float near, float far) {
  C3D_DepthMap(true, near - far, far);
}
static void viewport(int x, int y, int w, int h) {
  last_viewport[0]=x;last_viewport[1]=y;last_viewport[2]=w;last_viewport[3]=h;
  // The LCD is physically rotated. Logical GL viewport axes exchange places.
  C3D_SetViewport(y, 400 - x - w, h, w);
}
static void scissor(int x, int y, int w, int h) {
  last_scissor[0]=x;last_scissor[1]=y;last_scissor[2]=w;last_scissor[3]=h;
  int left = std::clamp(y, 0, 240), right = std::clamp(y + h, 0, 240);
  int bottom = std::clamp(400 - x - w, 0, 400),
      top = std::clamp(400 - x, 0, 400);
  C3D_SetScissor(GPU_SCISSOR_NORMAL, left, bottom, right, top);
}
static void alpha(bool use, bool modulate) {
  last_blend=use;
  (void)modulate;
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, use ? GPU_SRC_ALPHA : GPU_ONE,
                 use ? GPU_ONE_MINUS_SRC_ALPHA : GPU_ZERO, GPU_ONE,
                 GPU_ONE_MINUS_SRC_ALPHA);
}
struct Source {
  GPU_TEVSRC src;
  bool alpha;
  unsigned constant;
};
static Source source(unsigned id, unsigned channel) {
  if (id >= SHADER_INPUT_1 && id <= SHADER_INPUT_3) {
    int slot = id - SHADER_INPUT_1;
    if (slot == inputs.primary && channel && inputs.primary_fixed_alpha>=0)
      return {GPU_CONSTANT,false,(unsigned)inputs.primary_fixed_alpha};
    if (slot == inputs.primary)
      return {GPU_PRIMARY_COLOR, false, 0};
    if (slot == inputs.constant)
      return {GPU_CONSTANT, false, 2};
    if (slot == inputs.scalar) {
      if (channel && inputs.scalar_alpha==2)
        return {GPU_PRIMARY_COLOR,false,0};
      if (channel && inputs.scalar_alpha >= 0)
        return {GPU_CONSTANT, false, (unsigned)inputs.scalar_alpha};
      return {GPU_TEXTURE2, false, 0};
    }
  }
  switch (id) {
  case SHADER_0:
    return {GPU_CONSTANT, false, 0};
  case SHADER_1:
    return {GPU_CONSTANT, false, 1};
  case SHADER_INPUT_1:
    return {GPU_PRIMARY_COLOR, false, 0};
  case SHADER_INPUT_2:
    return {GPU_CONSTANT, false, 2};
  case SHADER_TEXEL0:
    return {GPU_TEXTURE0, false, 0};
  case SHADER_TEXEL0A:
    return {GPU_TEXTURE0, true, 0};
  case SHADER_TEXEL1:
    return {GPU_TEXTURE1, false, 0};
  case SHADER_TEXEL1A:
    return {GPU_TEXTURE1, true, 0};
  case SHADER_COMBINED:
    return {GPU_PREVIOUS, false, 0};
  default:
    unsupported("combiner input");
    return {};
  }
}
static void combiner_channel(C3D_TexEnv *env, unsigned cycle, unsigned channel,
                             const float constant[4], unsigned &constant_kind) {
  const auto &c = active->cc;
  PicaEquation equation;
  if (!pica_plan_equation(c,cycle,channel,&equation)) {
    unsupported("general combiner equation");
    return;
  }
  const unsigned *ids=equation.ids;
  const GPU_COMBINEFUNC functions[]={GPU_REPLACE,GPU_MODULATE,GPU_INTERPOLATE,GPU_MULTIPLY_ADD,GPU_ADD};
  GPU_COMBINEFUNC func=functions[equation.kind];
  Source s[3];
  for (unsigned i = 0; i < 3; ++i) {
    s[i] = source(ids[i], channel);
    if (s[i].src == GPU_CONSTANT) {
      s[i].constant=pica_constant_kind(s[i].constant,channel,constant);
      if (constant_kind != 99 && constant_kind != s[i].constant)
        unsupported("multiple TEV constants");
      constant_kind = s[i].constant;
    }
  }
  auto mode = channel ? C3D_Alpha : C3D_RGB;
  C3D_TexEnvSrc(env, mode, s[0].src, s[1].src, s[2].src);
  C3D_TexEnvFunc(env, mode, func);
  if (!channel)
    C3D_TexEnvOpRgb(
        env, s[0].alpha ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
        s[1].alpha ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR,
        s[2].alpha ? GPU_TEVOP_RGB_SRC_ALPHA : GPU_TEVOP_RGB_SRC_COLOR);
  else {
    auto op = [](Source value) {
      return value.src == GPU_TEXTURE2 ? GPU_TEVOP_A_SRC_R
                                       : GPU_TEVOP_A_SRC_ALPHA;
    };
    C3D_TexEnvOpAlpha(env, op(s[0]), op(s[1]), op(s[2]));
  }
  (void)constant;
}
static unsigned byte(float v) {
  return unsigned(std::clamp(v, 0.f, 1.f) * 255.f + .5f);
}
static void draw(float *buf, size_t floats, size_t triangles) {
  DrawCost cost;
  if (!active || !in_frame)
    sysFatalError("Native PICA: draw outside frame/shader");
  const auto &c = active->cc;
  if (c.num_inputs > 3 || c.opt_grayscale || c.opt_blur)
    unsupported("material features (see graphics log)");
  size_t count = triangles * 3;
  size_t stride =
      4 + (c.opt_fog ? 4 : 0) + (c.opt_alpha ? 4 : 3) * c.num_inputs;
  for (unsigned t = 0; t < 2; ++t)
    if (c.used_textures[t])
      stride += 2 + c.clamp[t][0] + c.clamp[t][1];
  if (floats != stride * count || vertex_count + count > MAX_VERTICES)
    sysFatalError("Native PICA: vertex layout/capacity");
  const unsigned channels = c.opt_alpha ? 4 : 3;
  const size_t input_offset = stride - channels * c.num_inputs;
  float fog_color[3] = {0, 0, 0};
  const size_t fog_offset = input_offset - (c.opt_fog ? 4 : 0);
  const u64 planning_start=svcGetSystemTick();
  bool mapped=pica_batch_fits(buf,stride,input_offset,triangles,c.num_inputs,c.opt_alpha,c.opt_fog,&inputs,fog_color);
  bool alpha_product=false;
  if(!mapped && (!c.opt_fog || pica_plan_fog(buf,stride,fog_offset,count,fog_color)))
    mapped=alpha_product=pica_alpha_product_plan(c,buf,stride,input_offset,count,&inputs);
  planning_ticks+=svcGetSystemTick()-planning_start;
  if(!mapped) {
    if(triangles>1) {
      size_t consumed=0;
      while(consumed<triangles) {
        float *part=buf+consumed*3*stride;
        const u64 split_start=svcGetSystemTick();
        size_t span=pica_batch_prefix(part,stride,input_offset,triangles-consumed,c.num_inputs,c.opt_alpha,c.opt_fog);
        planning_ticks+=svcGetSystemTick()-split_start;
        if(!span)span=1; // Single-triangle validation below reports its inputs.
        draw(part,span*3*stride,span);consumed+=span;
      }
      return;
    }
    for(unsigned input=0;input<(unsigned)c.num_inputs;++input)
      for(unsigned v=0;v<3;++v) {
        const float *p=buf+v*stride+input_offset+input*channels;
        sysLogPrintf(0,"Unmapped input %u vertex%u RGBA %.6f %.6f %.6f %.6f",input,v,
                     (double)p[0],(double)p[1],(double)p[2],c.opt_alpha?(double)p[3]:1.0);
      }
    unsupported("single-triangle color/fog layout");
  }
  PicaNoisePlan noise={};
  if(c.opt_noise && !pica_plan_noise(c,inputs.scalar>=0,&noise))
    unsupported("noise texture/stage capacity");
  float constant[4] = {1, 1, 1, 1};
  if (inputs.constant >= 0)
    memcpy(constant, buf + input_offset + inputs.constant * channels,
           channels * sizeof(float));
  if (!active->colors_logged) {
    active->colors_logged = true;
    sysLogPrintf(2,"PICA color plan %llx/%x: primary=%d constant=%d scalar=%d fogRGB=%.3f,%.3f,%.3f",
                 (unsigned long long)active->id0,active->id1,inputs.primary,
                 inputs.constant,inputs.scalar,(double)fog_color[0],
                 (double)fog_color[1],(double)fog_color[2]);
    for (unsigned input=0; input<(unsigned)c.num_inputs; ++input) {
      const float *sample=buf+input_offset+input*channels;
      sysLogPrintf(2,"  input%u RGBA=%.3f,%.3f,%.3f,%.3f",input,
                   (double)sample[0],(double)sample[1],(double)sample[2],
                   c.opt_alpha?(double)sample[3]:1.0);
    }
    if(c.opt_fog) {
      float low=1.f,high=0.f;
      for(size_t v=0;v<count;++v) {
        float factor=buf[v*stride+fog_offset+3];
        low=std::min(low,factor); high=std::max(high,factor);
      }
      sysLogPrintf(2,"  fog factor range=%.3f..%.3f",(double)low,(double)high);
    }
  }
  for (size_t i = 0; i < count; ++i) {
    float *p = buf + i * stride;
    Vertex &v = vertices[vertex_count + i];
    memcpy(v.pos, p, 16);
    p += 4;
    for (unsigned t = 0; t < 3; ++t)
      v.uv[t][0] = v.uv[t][1] = 0;
    for (unsigned t = 0; t < 2; ++t)
      if (c.used_textures[t]) {
        Texture *tex = texture(t);
        if (!tex->ready)
          sysFatalError("Native PICA: uninitialized texture");
        float u = *p++, vv = *p++;
        if (c.clamp[t][0])
          u = std::clamp(u, 0.f, *p++);
        if (c.clamp[t][1])
          vv = std::clamp(vv, 0.f, *p++);
        v.uv[t][0] = u * tex->width / tex->gpu.width;
        v.uv[t][1] = pica_texture_t(vv,tex->height,tex->gpu.height);
      }
    if (c.opt_fog) {
      v.uv[2][1] = pica_lookup_t(pica_ramp_coordinate(p[3]));
      p += 4;
    }
    if(c.opt_noise) {
      if(v.pos[3]<=0) unsupported("noise overlay behind the camera");
      float x=(v.pos[0]/v.pos[3]*.5f+.5f)*400.f;
      float y=(.5f-v.pos[1]/v.pos[3]*.5f)*240.f;
      v.uv[noise.texture][0]=(x+float((frame_count*73u)&255u))/256.f;
      v.uv[noise.texture][1]=1.f-(y+float((frame_count*151u)&255u))/256.f;
    }
    for (float &a : v.color)
      a = 1;
    if (c.num_inputs)
      memcpy(v.color, p + inputs.primary * channels, channels * sizeof(float));
    if (c.opt_alpha && inputs.primary_alpha>=0)
      v.color[3]=p[inputs.primary_alpha*channels+3];
    if(alpha_product)v.color[3]=p[3]*p[7];
    if (inputs.scalar >= 0) {
      v.uv[2][0] = pica_ramp_coordinate(p[inputs.scalar * channels]);
    }
  }
  ++draw_calls;
  if(frame_count%120==0)for(size_t i=0;i<count;++i) {
    const Vertex &v=vertices[vertex_count+i];
    if(v.color[0]>.1f || v.color[1]>.1f || v.color[2]>.1f)++bright_vertices;
    near_vertices+=v.pos[2]<-v.pos[3];far_vertices+=v.pos[2]>v.pos[3];behind_vertices+=v.pos[3]<=0;
    if(v.pos[3]>0 && std::fabs(v.pos[0])<=v.pos[3] && std::fabs(v.pos[1])<=v.pos[3] && std::fabs(v.pos[2])<=v.pos[3])++clip_vertices;
  }
  PicaTevKey key={active->id0,active->id1,inputs,
                 pica_constant_kind(2,0,constant),pica_constant_kind(2,1,constant)};
  if(!c.opt_noise && tev_valid && pica_tev_same(key,tev_key)) {
    ++tev_reuses;
  } else {
    ++tev_configs;
    C3D_TexEnvBufUpdate(C3D_Both,0);
    for(unsigned i=0;i<6;++i)C3D_TexEnvInit(C3D_GetTexEnv(i));
    for(unsigned cycle=0;cycle<(c.opt_2cyc?2u:1u);++cycle) {
      C3D_TexEnv *env=C3D_GetTexEnv(cycle);
      tev_rgb[cycle]=tev_alpha[cycle]=99;
      combiner_channel(env,cycle,0,constant,tev_rgb[cycle]);
      if(c.opt_alpha)combiner_channel(env,cycle,1,constant,tev_alpha[cycle]);
      else {
        C3D_TexEnvSrc(env,C3D_Alpha,GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env,C3D_Alpha,GPU_REPLACE);
      }
    }
    if(c.opt_fog) {
      C3D_TexEnv *env=C3D_GetTexEnv(c.opt_2cyc?2:1);
      C3D_TexEnvSrc(env,C3D_RGB,GPU_CONSTANT,GPU_PREVIOUS,GPU_TEXTURE2);
      C3D_TexEnvOpRgb(env,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_ALPHA);
      C3D_TexEnvFunc(env,C3D_RGB,GPU_INTERPOLATE);
      C3D_TexEnvSrc(env,C3D_Alpha,GPU_PREVIOUS);
      C3D_TexEnvFunc(env,C3D_Alpha,GPU_REPLACE);
    }
    tev_key=key;tev_valid=!c.opt_noise;
  }
  // Constants can change while the TEV wiring stays identical.
  for(unsigned cycle=0;cycle<(c.opt_2cyc?2u:1u);++cycle) {
    unsigned kind=tev_rgb[cycle],ak=tev_alpha[cycle];
    unsigned rgb=kind==2?byte(constant[0])|(byte(constant[1])<<8)|(byte(constant[2])<<16):kind==1?0xffffff:0;
    unsigned alpha=ak==2?byte(constant[3]):ak==1?255:0;
    C3D_TexEnvColor(C3D_GetTexEnv(cycle),rgb|(alpha<<24));
  }
  if(c.opt_fog)
    C3D_TexEnvColor(C3D_GetTexEnv(c.opt_2cyc?2:1),byte(fog_color[0])|(byte(fog_color[1])<<8)|(byte(fog_color[2])<<16));
  if(c.opt_noise) {
    const GPU_TEVSRC samples[]={GPU_TEXTURE0,GPU_TEXTURE1,GPU_TEXTURE2};
    // Store the fully combined alpha, never merely the vertex alpha. This
    // handles watch glyph textures and fades with the same path.
    C3D_TexEnvBufUpdate(C3D_Alpha,1u<<(noise.first-1));
    for(int stage=noise.first;stage<noise.last;++stage) {
      C3D_TexEnv *env=C3D_GetTexEnv(stage);
      C3D_TexEnvSrc(env,C3D_Alpha,GPU_PREVIOUS,samples[noise.texture]);
      C3D_TexEnvFunc(env,C3D_Alpha,stage==noise.first?GPU_SUBTRACT:GPU_REPLACE);
      C3D_TexEnvScale(env,C3D_Alpha,GPU_TEVSCALE_4);
    }
    C3D_TexEnv *env=C3D_GetTexEnv(noise.last);
    C3D_TexEnvSrc(env,C3D_Alpha,GPU_PREVIOUS,GPU_PREVIOUS_BUFFER);
    C3D_TexEnvFunc(env,C3D_Alpha,GPU_MODULATE);
    C3D_TexBind(noise.texture,&noise_samples[noise.scales-2]);
  }
  C3D_AlphaTest(c.opt_texture_edge || c.opt_alpha_threshold, GPU_GREATER,
                c.opt_texture_edge ? 127 : 0);
  for (unsigned t = 0; t < 2; ++t)
    if (c.used_textures[t])
      C3D_TexBind(t, &texture(t)->gpu);
  if (inputs.scalar >= 0 || c.opt_fog)
    C3D_TexBind(2, &scalar_ramp);
  C3D_DrawArrays(GPU_TRIANGLES, vertex_count, count);
  vertex_count += count;
  triangle_count += triangles;
}
static void init() {

  const unsigned flags = native_display_prepare();
  if (!C3D_Init(0x80000))
    sysFatalError("Native PICA: initialization failed");
  target =
      C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
  if (!target)
    sysFatalError("Native PICA: render target allocation failed");
  C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, flags);
  C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE,
                 GPU_ZERO);
  shader = DVLB_ParseFile((u32 *)native_clip_shbin, native_clip_shbin_size);
  if (!shader)
    sysFatalError("Native PICA: invalid shader");
  shaderProgramInit(&program);
  shaderProgramSetVsh(&program, &shader->DVLE[0]);
  if (!C3D_TexInit(&scalar_ramp, 256, 256, GPU_RGBA8))
    sysFatalError("Native PICA: scalar ramp allocation");
  auto *ramp = static_cast<unsigned char *>(scalar_ramp.data);
  for (unsigned y = 0; y < 256; ++y)
    for (unsigned x = 0; x < 256; ++x) {
      unsigned off = (((y / 8) * 32 + x / 8) * 64 + morton(x & 7, y & 7)) * 4;
      ramp[off] = y;
      ramp[off + 1] = ramp[off + 2] = ramp[off + 3] = x;
    }
  C3D_TexSetWrap(&scalar_ramp, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
  C3D_TexSetFilter(&scalar_ramp, GPU_LINEAR, GPU_LINEAR);
  C3D_TexFlush(&scalar_ramp);
  init_noise_samples();
  vertices = static_cast<Vertex *>(linearAlloc(MAX_VERTICES * sizeof(Vertex)));
  if (!vertices)
    sysFatalError("Native PICA: vertex allocation failed");
}
static void resize() {}
static void start() {
  gpu_wait_start=svcGetSystemTick();
  tev_valid=false;tev_configs=tev_reuses=0;
  __atomic_store_n(&gpu_phase,1,__ATOMIC_RELEASE);
  if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW))
    sysFatalError("Native PICA: frame begin failed");
  cpu_frame_start=svcGetSystemTick();
  draw_ticks=upload_ticks=planning_ticks=0;
  gpu_wait_ms=(double)(cpu_frame_start-gpu_wait_start)*1000.0/SYSCLOCK_ARM11;
  __atomic_store_n(&gpu_phase,2,__ATOMIC_RELEASE);
  // SYNCDRAW waits for the previous submissions before reusing vertex memory.
  for (auto &t : retired)
    C3D_TexDelete(&t);
  retired.clear();
  C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0x000000ff, 0);
  in_frame = true;
  vertex_count = 0;
  bright_vertices=clip_vertices=draw_calls=near_vertices=far_vertices=behind_vertices=0;
  C3D_FrameDrawOn(target);
  C3D_BindProgram(&program);
  C3D_Mtx tilt;
  Mtx_OrthoTilt(&tilt, -1, 1, -1, 1, 0, 1, true);
  C3D_FVUnifMtx4x4(
      GPU_VERTEX_SHADER,
      shaderInstanceGetUniformLocation(program.vertexShader, "tilt"), &tilt);
  C3D_AttrInfo *a = C3D_GetAttrInfo();
  AttrInfo_Init(a);
  AttrInfo_AddLoader(a, 0, GPU_FLOAT, 4);
  AttrInfo_AddLoader(a, 1, GPU_FLOAT, 4);
  AttrInfo_AddLoader(a, 2, GPU_FLOAT, 2);
  AttrInfo_AddLoader(a, 3, GPU_FLOAT, 2);
  AttrInfo_AddLoader(a, 4, GPU_FLOAT, 2);
  C3D_BufInfo *b = C3D_GetBufInfo();
  BufInfo_Init(b);
  BufInfo_Add(b, vertices, sizeof(Vertex), 5, 0x43210);
  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthMap(true, -1, 0);
  C3D_TexEnvBufUpdate(C3D_Both, 0);
}
static void end() {
  __atomic_store_n(&gpu_phase,3,__ATOMIC_RELEASE);
  if(frame_count%120==0)
    sysLogPrintf(2,"PICA frame %u: draws=%u vertices=%u bright=%u inClip=%u viewport=%d,%d,%d,%d scissor=%d,%d,%d,%d depth=%d blend=%d",
                 frame_count,draw_calls,(unsigned)vertex_count,bright_vertices,clip_vertices,
                 last_viewport[0],last_viewport[1],last_viewport[2],last_viewport[3],
                 last_scissor[0],last_scissor[1],last_scissor[2],last_scissor[3],last_depth,last_blend);
  if(frame_count%120==0)sysLogPrintf(2,"Render cost: cpu=%.2fms wait=%.2fms previousGPU=%.2fms cmd=%.1f%% TEVconfig=%u reuse=%u",
    (double)(svcGetSystemTick()-cpu_frame_start)*1000.0/SYSCLOCK_ARM11,gpu_wait_ms,
    (double)C3D_GetDrawingTime(),(double)C3D_GetCmdBufUsage()*100.0,tev_configs,tev_reuses);
  if(frame_count%120==0) {
    uint32_t cached,file;native_cart_stats(&cached,&file);
    sysLogPrintf(2,"Cartridge DMA: cached=%lu file=%lu",(unsigned long)cached,(unsigned long)file);
    const double ms=1000.0/SYSCLOCK_ARM11;
    sysLogPrintf(2,"Clip census: near=%u far=%u behind=%u",near_vertices,far_vertices,behind_vertices);
    sysLogPrintf(2,"Render breakdown: backend=%.2fms planning=%.2fms upload=%.2fms (wall times)",
                 draw_ticks*ms,planning_ticks*ms,upload_ticks*ms);
  }
  GSPGPU_FlushDataCache(vertices, vertex_count * sizeof(Vertex));
  C3D_FrameEnd(0);
  __atomic_store_n(&gpu_phase,0,__ATOMIC_RELEASE);
  in_frame = false;
  ++frame_count;
}
static void finish() {}
static int framebuffer() { return next_fb++; }
static void update_fb(int id, uint32_t w, uint32_t h, uint32_t msaa, bool, bool,
                      bool, bool) {
  if (id || w != 400 || h != 240 || msaa != 1)
    unsupported("offscreen framebuffer / resize");
}
static bool bind_fb(int id, float) {
  if (id)
    unsupported("offscreen framebuffer");
  return true;
}
static void clear_fb(bool color, bool depth) {
  C3D_RenderTargetClear(target,
                        (C3D_ClearBits)((color ? C3D_CLEAR_COLOR : 0) |
                                        (depth ? C3D_CLEAR_DEPTH : 0)),
                        0x000000ff, 0);
}
static void copy_fb(int, int, int, int, bool, bool) {
  unsupported("framebuffer copy");
}
static void resolve(int, int) { unsupported("MSAA resolve"); }
static void *fb_texture(int) {
  unsupported("framebuffer texture");
  return nullptr;
}
static void select_fb(int) { unsupported("framebuffer texture binding"); }
static void delete_texture(uint32_t id) {
  if (id >= textures.size() || !textures[id])
    return;
  if (textures[id]->ready)
    dispose(textures[id]->gpu);
  delete textures[id];
  textures[id] = nullptr;
}
static void filter(FilteringMode m) {
  if (m != FILTER_NONE && m != FILTER_LINEAR)
    unsupported("advanced filtering");
  filtering = m;
}
static FilteringMode get_filter() { return filtering; }
static void mipmap(MipmapFilteringMode m) { mip_filter = m; }
static void anisotropy(int n) {
  if (n > 1)
    unsupported("anisotropy");
}
static int max_anisotropy() { return 1; }
GfxRenderingAPI native_pica_api = {name,
                                   max_texture,
                                   clip,
                                   unload,
                                   load,
                                   create,
                                   lookup,
                                   info,
                                   clear_shaders,
                                   new_texture,
                                   select_texture,
                                   upload,
                                   sampler,
                                   depth,
                                   depth_range,
                                   viewport,
                                   scissor,
                                   alpha,
                                   draw,
                                   init,
                                   resize,
                                   start,
                                   end,
                                   finish,
                                   framebuffer,
                                   update_fb,
                                   bind_fb,
                                   copy_fb,
                                   clear_fb,
                                   resolve,
                                   fb_texture,
                                   select_fb,
                                   delete_texture,
                                   filter,
                                   get_filter,
                                   mipmap,
                                   anisotropy,
                                   max_anisotropy};
// This adapter exposes a fixed native LCD; window-only functions are
// deliberately absent, and the interpreter's actual call sites use only the
// functions below.
static void window_init(const GfxWindowInitSettings *) {}
static void dimensions(uint32_t *w, uint32_t *h, int32_t *x, int32_t *y) {
  *w = 400;
  *h = 240;
  if (x)
    *x = 0;
  if (y)
    *y = 0;
}
static void events() {}
static bool window_start() { return true; }
static void swap() {}
static int target_fps = 60;
static int32_t get_fps() { return target_fps; }
static void set_fps(int fps) { target_fps = fps; }
GfxWindowManagerAPI native_window_api = [] {
  GfxWindowManagerAPI a = {};
  a.init = window_init;
  a.get_dimensions = dimensions;
  a.handle_events = events;
  a.start_frame = window_start;
  a.swap_buffers_begin = swap;
  a.swap_buffers_end = swap;
  a.get_target_fps = get_fps;
  a.set_target_fps = set_fps;
  return a;
}();
extern "C" unsigned native_graphics_frames() { return frame_count; }
extern "C" unsigned native_graphics_triangles() { return triangle_count; }
extern "C" unsigned native_graphics_uploads() { return upload_count; }
extern "C" void native_graphics_shutdown() {
  if (in_frame)
    end();
  // Detaching outside a frame waits for the GPU transfer/draw queue, before
  // texture memory used by the last room frame can be freed.
  C3D_RenderTargetDetachOutput(target);
  native_graphics_destroy_interpreter();
  for (unsigned i = 1; i < textures.size(); ++i)
    delete_texture(i);
  textures.clear();
  for (auto &t : retired)
    C3D_TexDelete(&t);
  retired.clear();
  clear_shaders();
  C3D_TexDelete(&scalar_ramp);
  for(auto &tex:noise_samples) C3D_TexDelete(&tex);
  linearFree(vertices);
  vertices = nullptr;
  shaderProgramFree(&program);
  DVLB_Free(shader);
  C3D_RenderTargetDelete(target);
  target = nullptr;
  C3D_Fini();
}

/* Called only after engine quit and renderer idle. Keep engine-owned caches
 * allocated until process exit, but remove Citro3D GSP/APT callbacks first. */
extern "C" void native_graphics_stop_presenting(void) {
  C3D_RenderTargetDetachOutput(target);
  C3D_Fini();
}
