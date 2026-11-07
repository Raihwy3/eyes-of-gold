#include "native_stereo_options.h"
/* First PICA backend for the original Fast3D interpreter. No recreation code.
 * Unsupported material/framebuffer features fail explicitly during bring-up.
 */
#include "gfx_cc.h"
#include "gfx_rendering_api.h"
#include "gfx_window_manager_api.h"
#include "native_clip_shbin.h"
#include "native_world_shbin.h"
#include "native_gpu_world.h"
#include "native_display.h"
#include "native_graphics.h"
#include "native_weapon_stereo.h"
#include "native_dynamic_stereo.h"
#include "native_work_profile.h"
#include "pica_depth.h"
#include "pica_fog.h"
#include "pica_inputs.h"
#include "pica_lod_inputs.h"
#include "pica_proven_inputs.h"
#include "pica_batch.h"
#include "pica_alpha_product.h"
#include "pica_tev_cache.h"
#include "pica_texcoord.h"
#include "pica_noise.h"
#include "pica_decal_depth.h"
#include "native_cart.h"
#include "native_settings.h"
#include "native_bottom.h"
#include "native_watch_mirror.h"
#include "native_theme.h"
#include "pica_combiner.h"
#include <3ds.h>
#include "native_rsp_profile.h"
#ifdef __3DS__
static_assert(native_rsp_profile::tick_frequency==SYSCLOCK_ARM11,"RSP timing clock must match this SDK");
#endif
#include <algorithm>
#include <citro3d.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" void sysFatalError(const char *, ...) __attribute__((noreturn));
extern "C" void native_graphics_destroy_interpreter(void);
extern "C" void native_platform_log_batch_begin(void) __attribute__((weak));
extern "C" void native_platform_log_batch_end(void) __attribute__((weak));
extern "C" void sysLogPrintf(int, const char *, ...);
struct ShaderProgram {
  uint64_t id0;
  uint32_t id1;
  CCFeatures cc;
  bool colors_logged;
};
struct Texture {
  C3D_Tex gpu;
  unsigned width, height, levels, lod_extra_bytes;
  bool ready, linear, requested_mips, lod_failed;
  unsigned cms, cmt;
};
struct Vertex {
  float pos[4], color[4], uv[3][2];
};
// IDs are valid only during the synchronous producer draw. They identify
// complete packed vertices, not model addresses or approximate attributes.
static const float *source_vertex_base;
static const uint64_t *source_vertex_ids;
static size_t source_vertex_count;
static const float *reference_vertex_base;
static const float *const *reference_vertices;
static size_t reference_vertex_count;
extern "C" void native_pica_vertex_refs(const float *base,const float *const *refs,size_t count) {
 reference_vertex_base=base;reference_vertices=refs;reference_vertex_count=count;
}

extern "C" void native_pica_vertex_ids(const float *base,const uint64_t *ids,size_t count) {
  source_vertex_base=base;source_vertex_ids=ids;source_vertex_count=count;
}
static struct ConvertedVertexSlot {uint64_t id;size_t index;} converted_slots[128];
static unsigned converted_reused,converted_built;
#include "pica_static_attributes.h"
static PicaStaticAttributes<Vertex> static_attributes;
static const float *attribute_source_base;
static const uint64_t *attribute_source_ids;
static size_t attribute_source_count;
extern "C" void native_pica_attribute_ids(const float *base,const uint64_t *ids,size_t count) {
 attribute_source_base=base;attribute_source_ids=ids;attribute_source_count=count;
}
#include "pica_mesh_cache.h"
static PicaMeshCache mesh_cache;
#include "pica_retained_mesh.h"
static PicaRetainedMesh retained_meshes;
static uint64_t retained_token;
static bool retained_stream_enabled(){static const bool enabled=getenv("EOG_DISABLE_RETAINED_STREAMING")==nullptr;return enabled;}
extern "C" void native_pica_retained_hint(uint64_t token){retained_token=token;}
extern "C" int native_pica_retained_supported(void){
  static const bool disabled=getenv("EOG_DISABLE_RETAINED_MESHES")!=nullptr;
  return !disabled && retained_meshes.available();
}
static C3D_RenderTarget *target,*watch_target,*right_target;
static float stereo_slider;
static bool stereo_active,ui_rect_scope,frontend_ui,viewmodel_scope;
static int top_sample_scale=1;
static unsigned top_output_flags;
static C3D_Mtx stereo_tilt[11]; // flat, UI, first-person and world left/right
static void flush_draw();
static native_stereo::Convergence auto_convergence;
static native_stereo::AimDepth aim_depth;
static void queue_stereo_depth();
static uint32_t *depth_copy;
static bool depth_copied,depth_copy_failed,sky_scope;
static constexpr unsigned DEPTH_COPY_CAPACITY=512*512*4;
static native_stereo::Projection stereo_projection;
static float world_convergence=native_stereo::manual_convergence,reticle_shift;
static bool reticle_scope,reticle_seen;
static float reticle_x,reticle_y;
struct StereoDepthFrame {
 C3D_RenderTarget *target=nullptr;native_stereo::Projection projection;
 float scale=-1,bias=0,convergence=native_stereo::manual_convergence,slider=0;int viewport[4]={};
 C3D_Mtx tilt={};unsigned triangles=0,bytes=0;bool usable=false;
 const uint32_t *pixels=nullptr;unsigned width=0,height=0;int samples=1;
};
static StereoDepthFrame depth_frame;
extern "C" void native_pica_projection(float a,float b,float c,float d){
 auto next=native_stereo::Projection::from(a,b,c,d);
 if(next.valid!=stereo_projection.valid || next.ratio!=stereo_projection.ratio || next.b!=stereo_projection.b)flush_draw();
 stereo_projection=next;
}

extern "C" void native_pica_frontend_ui(int enabled){frontend_ui=enabled!=0;}
extern "C" int native_pica_ui_rect(int enabled);
static Vertex *watch_vertices;
static C3D_BufInfo top_buffer,watch_buffer;
static bool watch_scope,watch_tabs,watch_body,watch_seen,watch_attached;
static unsigned watch_tab_triangles;
static bool watch_rect_active;
static unsigned watch_rect_vertices;
static float watch_rect_xy[4],watch_canvas_scale=1.f,watch_canvas_offset;
static int watch_tab_bounds[5][4];
static void mirror_draw(int first,int count);
static DVLB_s *shader;
static shaderProgram_s program;
static int tilt_uniform,clip_tilt_uniform,world_tilt_uniform,world_matrix_uniform;
static int clip_stereo_uniform,world_stereo_uniform;
static DVLB_s *world_shader;
static shaderProgram_s world_program;
static bool gpu_world;
static unsigned gpu_world_draws;
static constexpr unsigned WORLD_PALETTE_SIZE=20;
static bool world_palette_active;
static unsigned world_palette_count,world_palette_row,world_palette_cull;
static unsigned world_palette_switches,world_palette_continued,world_palette_rollovers;
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
static PicaDepthPlan sharp_depth={false,false,false,0};
static float sharp_depth_scale=-1,sharp_depth_bias;
static void restore_top_view() {
  C3D_SetViewport(last_viewport[1]*top_sample_scale,400-last_viewport[0]-last_viewport[2],last_viewport[3]*top_sample_scale,last_viewport[2]);
  C3D_SetScissor(GPU_SCISSOR_NORMAL,std::clamp(last_scissor[1],0,240)*top_sample_scale,std::clamp(400-last_scissor[0]-last_scissor[2],0,400),
    std::clamp(last_scissor[1]+last_scissor[3],0,240)*top_sample_scale,std::clamp(400-last_scissor[0],0,400));
}


static Vertex *vertices;
static size_t vertex_count;
static uint16_t *world_indices;
static size_t world_index_count;
static bool pending_indexed;
static const uint16_t *pending_elements;
static unsigned indexed_draws,indexed_saved;
static unsigned retained_streamed_parts;
static unsigned cpu_array_draws;
static bool cpu_world_hint;
extern "C" void native_pica_cpu_world_hint(int enabled){cpu_world_hint=enabled!=0;}
static constexpr unsigned MAX_VERTICES = 32768;
static FilteringMode filtering = FILTER_NONE;
static MipmapFilteringMode mip_filter = MIPMAP_DISABLED;
static C3D_Tex scalar_ramp;
static C3D_Tex noise_samples[2];
static PicaInputPlan inputs;
static bool tev_valid;
static PicaTevKey tev_key;
struct MaterialSetup {C3D_TexEnv env[6];unsigned rgb[2],alpha[3];bool signed_alpha;};
static PicaTevCache<MaterialSetup> material_cache;
static unsigned tev_configs,tev_reuses,material_hits,material_builds;
static PicaOrderedDraw pending_draw;
static unsigned gpu_draws,merged_draws;
static void flush_draw() {
  if(!pending_draw.count)return;
  const bool flat_overlay=watch_body; // physical watch and screen-space gauges
  const bool raised=(frontend_ui && !gpu_world) || watch_scope || ui_rect_scope || reticle_scope;
  const unsigned left_eye=flat_overlay?0:sky_scope?9:reticle_scope?7:viewmodel_scope?3:raised?1:5;
  const unsigned right_eye=flat_overlay?0:sky_scope?10:reticle_scope?8:viewmodel_scope?4:raised?2:6;
  const float eye_translation=stereo_active && !flat_overlay && !raised && !sky_scope?native_stereo_eye_translation(stereo_slider,false,viewmodel_scope,world_convergence):0.f;
  const int stereo_uniform=world_palette_active?world_stereo_uniform:clip_stereo_uniform;
  // World geometry must retain the full off-axis eye projection. Clamping
  // each vertex separately collapses all close surfaces onto one depth plane.
  const float full_world_depth=stereo_active && !flat_overlay && !raised && !viewmodel_scope && !sky_scope?1.f:0.f;
  const float shader_full_depth=sky_scope?1.f:full_world_depth;
  // Choose the dominant depth-writing world projection. Menus, weapons and
  // the watch have separate cameras and cannot supply scene depth.
  if(full_world_depth && !frontend_ui && !depth_copied && stereo_projection.valid && sharp_depth.test && sharp_depth.write &&
     last_viewport[2]>=280 && last_viewport[3]>=180 &&
     pending_draw.count>depth_frame.triangles){
    depth_frame.target=target;depth_frame.projection=stereo_projection;
    depth_frame.scale=sharp_depth_scale;depth_frame.bias=sharp_depth_bias;
    depth_frame.convergence=world_convergence;depth_frame.slider=stereo_slider;
    depth_frame.triangles=pending_draw.count;depth_frame.usable=true;
    memcpy(depth_frame.viewport,last_viewport,sizeof last_viewport);depth_frame.tilt=stereo_tilt[0];
    depth_frame.samples=top_sample_scale;depth_frame.width=target->frameBuf.width;depth_frame.height=target->frameBuf.height;
  }
  const float shift_limit=reticle_scope?.30f:stereo_slider*.03f;
  C3D_FVUnifSet(GPU_VERTEX_SHADER,stereo_uniform,stereo_active?shift_limit:0.f,eye_translation,shader_full_depth,0);
  if(stereo_active)C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,tilt_uniform,&stereo_tilt[left_eye]);
  if(pending_indexed)C3D_DrawElements(GPU_TRIANGLES,pending_draw.count,C3D_UNSIGNED_SHORT,(pending_elements?pending_elements:world_indices)+pending_draw.first);
  else C3D_DrawArrays(GPU_TRIANGLES,pending_draw.first,pending_draw.count);
  if(stereo_active) {
    C3D_FrameDrawOn(right_target);restore_top_view();
    C3D_FVUnifSet(GPU_VERTEX_SHADER,stereo_uniform,shift_limit,-eye_translation,shader_full_depth,0);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,tilt_uniform,&stereo_tilt[right_eye]);
    if(pending_indexed)C3D_DrawElements(GPU_TRIANGLES,pending_draw.count,C3D_UNSIGNED_SHORT,(pending_elements?pending_elements:world_indices)+pending_draw.first);
    else C3D_DrawArrays(GPU_TRIANGLES,pending_draw.first,pending_draw.count);
    C3D_FrameDrawOn(target);restore_top_view();
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,tilt_uniform,&stereo_tilt[0]);
    // Direct bottom-screen and postprocess draws use the restored flat camera.
    C3D_FVUnifSet(GPU_VERTEX_SHADER,stereo_uniform,0,0,0,0);
  }
  if(!pending_indexed)mirror_draw(pending_draw.first,pending_draw.count);
  ++gpu_draws;pending_draw.clear();pending_indexed=false;pending_elements=nullptr;
}
extern "C" void native_pica_reticle(int enabled){
 if(reticle_scope==(enabled!=0))return;
 flush_draw();reticle_scope=enabled!=0;
}
extern "C" void native_pica_sky(int enabled){
 if(sky_scope==(enabled!=0))return;
 flush_draw();sky_scope=enabled!=0;
}
extern "C" int native_pica_gpu_world_supported(void){
  static const bool disabled=getenv("EOG_DISABLE_GPU_WORLD")!=nullptr;
  return world_indices && !disabled;
}
extern "C" int native_pica_gpu_world(const NativeGpuWorld *world) {
  // Producers frequently end an already inactive model scope. Keep clip draws
  // queued instead of flushing and rebinding the same shader for these no-ops.
  if(!world && !world_palette_active)return 1;
  // Appending immutable matrix rows cannot affect vertices already pending.
  // Only changes to shader/culling or recycling a full palette are barriers.
  bool entering=world && !world_palette_active;
  bool cull_change=world && world_palette_active && world->cull!=world_palette_cull;
  static const bool single_matrix=getenv("EOG_DISABLE_MATRIX_PALETTE")!=nullptr;
  bool full=world && world_palette_active && (world_palette_count==WORLD_PALETTE_SIZE || single_matrix);
  bool barrier=!world || entering || cull_change || full;
  if(barrier)flush_draw();
  if(full)++world_palette_rollovers;
  if(world){++world_palette_switches;if(!barrier && pending_draw.count)++world_palette_continued;}
  if(!world || entering) {
    C3D_BindProgram(world?&world_program:&program);
    tilt_uniform=world?world_tilt_uniform:clip_tilt_uniform;
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,tilt_uniform,&stereo_tilt[0]);
  }
  if(barrier)world_palette_count=0;
  world_palette_row=world_palette_count*4;
  // Form PICA depth BEFORE the shader's float24 matrix multiply. Subtracting
  // separately transformed clip Z/W loses the small depth difference at
  // world-scale distances and makes GPU meshes fight CPU-clipped surfaces.
  float rows[4][4];
  if(world) {
    memcpy(rows,world->rows,sizeof rows);
    for(unsigned col=0;col<4;col++)rows[2][col]=float((double(world->rows[2][col])-world->rows[3][col])*.5);
  }
  if(world)for(unsigned row=0;row<4;row++)
    C3D_FVUnifSet(GPU_VERTEX_SHADER,world_matrix_uniform+world_palette_row+row,
      rows[row][0],rows[row][1],rows[row][2],rows[row][3]);
  if(world){++world_palette_count;world_palette_cull=world->cull;}
  if(!world || entering || cull_change)
    C3D_CullFace(!world || !world->cull?GPU_CULL_NONE:world->cull==1?GPU_CULL_FRONT_CCW:GPU_CULL_BACK_CCW);
  gpu_world=world_palette_active=world!=nullptr;
  return 1;
}
extern "C" int native_pica_ui_rect(int enabled) {
  if(!stereo_active)return 0;
  if(enabled<0)return 1;
  if(ui_rect_scope==(enabled!=0))return 1;
  flush_draw();ui_rect_scope=enabled!=0;return 1;
}
#include "pica_watch_mirror.h"
static unsigned texture_lod_level,texture_lod_draws[4],texture_lod_promotions,texture_lod_extra_live;
static constexpr unsigned texture_lod_budget=512*1024;
extern "C" void native_pica_texture_lod(unsigned level) {
  level=std::min(level,3u);if(level==texture_lod_level)return;
  flush_draw();texture_lod_level=level;
}
extern "C" void native_pica_set_viewmodel(int enabled) {
  if(viewmodel_scope==(enabled!=0))return;
  flush_draw();
  if(enabled)queue_stereo_depth();
  viewmodel_scope=enabled!=0;
}
static u64 gpu_wait_start,cpu_frame_start;
static double gpu_wait_ms;
static u64 draw_ticks,upload_ticks,planning_ticks;
static unsigned draw_depth;
static bool measure_frame;
struct DrawCost {
  u64 start;bool outer;
  DrawCost():start(measure_frame?svcGetSystemTick():0),outer(draw_depth++==0) {}
  ~DrawCost(){--draw_depth;if(outer && measure_frame){u64 ticks=svcGetSystemTick()-start;draw_ticks+=ticks;native_rsp_profile::note_backend(ticks);}}
};
struct UploadCost {
  u64 start=measure_frame?svcGetSystemTick():0;
  ~UploadCost(){if(measure_frame){u64 ticks=svcGetSystemTick()-start;upload_ticks+=ticks;if(!draw_depth)native_rsp_profile::note_backend(ticks);}}
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
static void release_texture_lod_budget(Texture *tex) {
  texture_lod_extra_live-=tex->lod_extra_bytes;tex->lod_extra_bytes=0;
}
static void upload(const uint8_t *rgba, uint32_t w, uint32_t h, bool mipmaps) {
  flush_draw();
  UploadCost cost;
  if (!w || !h || w > 1024 || h > 1024)
    sysFatalError("Native PICA: texture size %ux%u", w, h);
  Texture *t = texture(upload_tile);
  if (t->ready){release_texture_lod_budget(t);dispose(t->gpu);}
  unsigned pw = power2(w), ph = power2(h);
  if (!(mipmaps ? C3D_TexInitMipmap(&t->gpu, pw, ph, GPU_RGBA8)
                : C3D_TexInit(&t->gpu, pw, ph, GPU_RGBA8)))
    sysFatalError("Native PICA: texture allocation failed");
  t->lod_failed=false;
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
static void ensure_texture_lods(Texture *tex) {
  if(!texture_lod_level || !tex->ready || tex->lod_failed)return;
  int available=std::min(3,C3D_TexCalcMaxLevel(tex->gpu.width,tex->gpu.height));
  if(available<=0 || tex->levels>=unsigned(available))return;
  unsigned extra=C3D_TexCalcTotalSize(tex->gpu.size,available)-C3D_TexCalcTotalSize(tex->gpu.size,tex->levels);
  if(texture_lod_promotions>=4 || extra>texture_lod_budget-texture_lod_extra_live)return;
  C3D_Tex replacement={};
  C3D_TexInitParams params={tex->gpu.width,tex->gpu.height,(u8)available,GPU_RGBA8,GPU_TEX_2D,false};
  if(!C3D_TexInitWithParams(&replacement,nullptr,params)){tex->lod_failed=true;return;}
  flush_draw();
  memcpy(replacement.data,tex->gpu.data,tex->gpu.size);
  replacement.param=tex->gpu.param;replacement.border=tex->gpu.border;
  C3D_TexGenerateMipmap(&replacement,GPU_TEXFACE_2D);C3D_TexFlush(&replacement);
  dispose(tex->gpu);tex->gpu=replacement;tex->levels=available;++texture_lod_promotions;
  texture_lod_extra_live+=extra;tex->lod_extra_bytes+=extra;
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
  unsigned forced=std::min(texture_lod_level,tex->levels);
  tex->gpu.minLevel=forced;
  tex->gpu.maxLevel = forced ? forced :
      tex->requested_mips && mip_filter != MIPMAP_DISABLED ? tex->levels : 0;
  C3D_TexSetFilterMipmap(&tex->gpu, mip_filter == MIPMAP_LINEAR ? GPU_LINEAR
                                                                : GPU_NEAREST);
  C3D_TexSetWrap(&tex->gpu, wrap(tex->cms), wrap(tex->cmt));
}
static void sampler(int tile, bool linear, uint32_t s, uint32_t t, bool mip) {
  flush_draw();
  Texture *tex = texture(tile);
  tex->linear = linear;
  tex->cms = s;
  tex->cmt = t;
  tex->requested_mips = mip;
  apply_sampler(tex);
}

static void depth(bool test, bool write, bool compare, bool prim,
                  uint16_t mode) {
  flush_draw();
  last_depth=test;
  PicaDepthPlan plan;
  if (!pica_plan_depth(test, write, compare, prim, mode, &plan))
    unsupported("depth mode");
  sharp_depth=plan;sharp_depth_scale=-1;sharp_depth_bias=plan.bias;
  C3D_DepthTest(plan.test, plan.compare ? (plan.inclusive?GPU_GEQUAL:GPU_GREATER) : GPU_ALWAYS,
                plan.write ? GPU_WRITE_ALL : GPU_WRITE_COLOR);
  C3D_DepthMap(true, -1.f, plan.bias);
}

static void depth_range(float near, float far) {
  flush_draw();
  sharp_depth_scale=near-far;sharp_depth_bias=far;
  C3D_DepthMap(true, near - far, far);
}
static void viewport(int x, int y, int w, int h) {
  flush_draw();
  last_viewport[0]=x;last_viewport[1]=y;last_viewport[2]=w;last_viewport[3]=h;
  // The LCD is physically rotated. Logical GL viewport axes exchange places.
  C3D_SetViewport(y*top_sample_scale, 400 - x - w, h*top_sample_scale, w);
}
static void scissor(int x, int y, int w, int h) {
  flush_draw();
  last_scissor[0]=x;last_scissor[1]=y;last_scissor[2]=w;last_scissor[3]=h;
  int left = std::clamp(y, 0, 240), right = std::clamp(y + h, 0, 240);
  int bottom = std::clamp(400 - x - w, 0, 400),
      top = std::clamp(400 - x, 0, 400);
  C3D_SetScissor(GPU_SCISSOR_NORMAL, left*top_sample_scale, bottom, right*top_sample_scale, top);
}
static void alpha(bool use, bool modulate) {
  flush_draw();
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
    if(slot==2 && inputs.zero_third)return {GPU_CONSTANT,false,0};
    if (slot == inputs.primary && channel && inputs.primary_fixed_alpha>=0)
      return {GPU_CONSTANT,false,(unsigned)inputs.primary_fixed_alpha};
    if (slot == inputs.primary)
      return {GPU_PRIMARY_COLOR, false, 0};
    if (slot == inputs.scalar) {
      if (channel && inputs.scalar_alpha==3)
        return {GPU_CONSTANT,false,2};
      if (channel && inputs.scalar_alpha==2)
        return {GPU_PRIMARY_COLOR,false,0};
      if (channel && inputs.scalar_alpha >= 0)
        return {GPU_CONSTANT, false, (unsigned)inputs.scalar_alpha};
      return {GPU_TEXTURE2, false, 0};
    }
    if (slot == inputs.constant)
      return {GPU_CONSTANT, false, 2};
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
  // Debris alpha: (A-B)*tex0, then 1*zero+combined. The zero may
  // occupy a real uniform input slot or come from the reduced input plan.
  // Use the two alpha stages for subtract then multiply; early clamping
  // is equivalent because the texture factor is nonnegative.
  const auto *first=c.c[0][1],*last=c.c[1][1];
  if(channel && c.opt_2cyc &&
     first[0]==SHADER_INPUT_1 && first[1]==SHADER_INPUT_2 && first[2]==SHADER_TEXEL0 && first[3]==SHADER_0 &&
     last[0]==SHADER_1 && last[1]==SHADER_0 && last[2]==SHADER_INPUT_3 && last[3]==SHADER_COMBINED) {
    Source a=source(SHADER_INPUT_1,1),b=source(SHADER_INPUT_2,1),third=source(SHADER_INPUT_3,1);
    bool zero=third.src==GPU_CONSTANT && pica_constant_kind(third.constant,1,constant)==0;
    bool identity=a.src==GPU_CONSTANT && pica_constant_kind(a.constant,1,constant)==1;
    // The literal-one case uses the existing one-minus operand below.
    if(zero && !identity) {
      if(cycle==0) {
        for(Source value:{a,b})if(value.src==GPU_CONSTANT) {
          unsigned kind=pica_constant_kind(value.constant,1,constant);
          if(constant_kind!=99 && constant_kind!=kind)unsupported("multiple debris alpha constants");
          constant_kind=kind;
        }
        C3D_TexEnvSrc(env,C3D_Alpha,a.src,b.src);
        C3D_TexEnvFunc(env,C3D_Alpha,GPU_SUBTRACT);
        auto operand=[](Source value){return value.src==GPU_TEXTURE2?GPU_TEVOP_A_SRC_R:GPU_TEVOP_A_SRC_ALPHA;};
        C3D_TexEnvOpAlpha(env,operand(a),operand(b));
      } else {
        C3D_TexEnvSrc(env,C3D_Alpha,GPU_PREVIOUS,GPU_TEXTURE0);
        C3D_TexEnvFunc(env,C3D_Alpha,GPU_MODULATE);
        C3D_TexEnvOpAlpha(env,GPU_TEVOP_A_SRC_ALPHA,GPU_TEVOP_A_SRC_ALPHA);
      }
      return;
    }
  }
  PicaEquation equation;
  bool inverse_alpha=false;
  if (!pica_plan_equation(c,cycle,channel,&equation)) {
    const auto *t=c.c[cycle][channel];
    Source a=source(t[0],channel);
    if(channel && t[3]==SHADER_0 && a.src==GPU_CONSTANT &&
       pica_constant_kind(a.constant,channel,constant)==1) {
      equation={PICA_MULTIPLY,{t[1],t[2],t[2]}};
      inverse_alpha=true;
    } else unsupported("general combiner equation");
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
    C3D_TexEnvOpAlpha(env, inverse_alpha?(s[0].src==GPU_TEXTURE2?GPU_TEVOP_A_ONE_MINUS_SRC_R:GPU_TEVOP_A_ONE_MINUS_SRC_ALPHA):op(s[0]), op(s[1]), op(s[2]));
  }
  (void)constant;
}
// The wreckage combiner adds C after (A-B)*texture alpha. Preserve both signs
// using positive/negative parts; a plain SUBTRACT would clamp away B-A.
static bool signed_debris_alpha(const float constant[4]) {
  const auto &c=active->cc;
  const auto *first=c.c[0][1],*last=c.c[1][1];
  if(!c.opt_alpha || !c.opt_2cyc ||
     first[0]!=SHADER_INPUT_1 || first[1]!=SHADER_INPUT_2 || first[2]!=SHADER_TEXEL0 || first[3]!=SHADER_0 ||
     last[0]!=SHADER_1 || last[1]!=SHADER_0 || last[2]!=SHADER_INPUT_3 || last[3]!=SHADER_COMBINED)return false;
  Source third=source(SHADER_INPUT_3,1);
  Source a=source(SHADER_INPUT_1,1);
  // The existing one-minus path already handles literal A=1 in two stages.
  if(a.src==GPU_CONSTANT && pica_constant_kind(a.constant,1,constant)==1)return false;
  return third.src!=GPU_CONSTANT || pica_constant_kind(third.constant,1,constant)!=0;
}
static void configure_signed_debris(MaterialSetup &built,const float constant[4]) {
  if(active->cc.opt_noise)unsupported("signed debris alpha with noise");
  Source a=source(SHADER_INPUT_1,1),b=source(SHADER_INPUT_2,1),add=source(SHADER_INPUT_3,1);
  Source previous={GPU_PREVIOUS,false,0},tex={GPU_TEXTURE0,false,0},buffer={GPU_PREVIOUS_BUFFER,false,0};
  auto stage=[&](unsigned index,GPU_COMBINEFUNC function,Source x,Source y,Source z) {
    auto *env=&built.env[index];
    unsigned kind=99;
    for(Source value:{x,y,z})if(value.src==GPU_CONSTANT) {
      unsigned next=pica_constant_kind(value.constant,1,constant);
      if(kind!=99 && kind!=next)unsupported("multiple signed debris constants");
      kind=next;
    }
    if(index<3)built.alpha[index]=kind;
    else if(kind!=99)unsupported("late signed debris constant");
    C3D_TexEnvSrc(env,C3D_Alpha,x.src,y.src,z.src);
    C3D_TexEnvFunc(env,C3D_Alpha,function);
    auto operand=[](Source s){return s.src==GPU_TEXTURE2?GPU_TEVOP_A_SRC_R:GPU_TEVOP_A_SRC_ALPHA;};
    C3D_TexEnvOpAlpha(env,operand(x),operand(y),operand(z));
  };
  auto difference=[&](unsigned index,Source x,Source y) {
    auto literal=[&](Source value,unsigned kind) {
      return value.src==GPU_CONSTANT && pica_constant_kind(value.constant,1,constant)==kind;
    };
    if(literal(x,1)) {
      stage(index,GPU_REPLACE,y,previous,previous);
      C3D_TexEnvOpAlpha(&built.env[index],y.src==GPU_TEXTURE2?GPU_TEVOP_A_ONE_MINUS_SRC_R:GPU_TEVOP_A_ONE_MINUS_SRC_ALPHA);
    } else if(literal(x,0) || literal(y,1))
      stage(index,GPU_REPLACE,{GPU_CONSTANT,false,0},previous,previous);
    else if(literal(y,0))stage(index,GPU_REPLACE,x,previous,previous);
    else stage(index,GPU_SUBTRACT,x,y,previous);
  };
  difference(0,a,b);                                // positive part
  stage(1,GPU_MULTIPLY_ADD,previous,tex,add);          // save positive*T+C
  difference(2,b,a);                                // negative part
  stage(3,GPU_MODULATE,previous,tex,previous);
  stage(4,GPU_SUBTRACT,buffer,previous,previous);      // saved - negative*T
  built.signed_alpha=true;
}
static unsigned byte(float v) {
  return unsigned(std::clamp(v, 0.f, 1.f) * 255.f + .5f);
}
// A proof from the Fast3D packer, valid only during its synchronous draw.
static bool uniform_fog_hint;
static float uniform_fog_colour[3];
extern "C" void native_pica_fog_hint(int valid,const float *colour) {
  static const bool disabled=std::getenv("EOG_DISABLE_FOG_BATCH_HINT")!=nullptr;
  uniform_fog_hint=valid && !disabled;
  if(uniform_fog_hint)memcpy(uniform_fog_colour,colour,sizeof uniform_fog_colour);
}
static bool plan_batch_fog(const float *buf,size_t stride,size_t offset,size_t count,float *colour,const float *const *refs=nullptr) {
  if(!uniform_fog_hint)return pica_plan_fog(buf,stride,offset,count,colour,refs);
  if(!count || offset+4>stride)return false;
  memcpy(colour,uniform_fog_colour,sizeof uniform_fog_colour);
  // RGB was proven by the producer; per-vertex fog factors still need checking.
  for(size_t v=0;v<count;v++)if(!__builtin_isfinite(pica_vertex_at(buf,stride,v,refs)[offset+3]))return false;
  return true;
}
static unsigned lod_rgb_hint,lod_alpha_hint,mesh_camera_lod_draws,mesh_camera_lod_hits;
static unsigned uniform_input_hint,alpha_uniform_hint,proven_input_draws,scanned_input_draws;
extern "C" void native_pica_input_hint(unsigned uniform,unsigned alpha){uniform_input_hint=uniform;alpha_uniform_hint=alpha;}

extern "C" void native_pica_lod_hint(unsigned rgb,unsigned alpha){lod_rgb_hint=rgb;lod_alpha_hint=alpha;}
static bool mesh_cache_enabled=getenv("EOG_DISABLE_GPU_MESH_CACHE")==nullptr;
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
  const float *const *refs=nullptr;
  if(reference_vertices && reference_vertex_base) {
    uintptr_t begin=reinterpret_cast<uintptr_t>(reference_vertex_base),at=reinterpret_cast<uintptr_t>(buf);
    if(at>=begin && (at-begin)%(stride*sizeof(float))==0) {
      size_t offset=(at-begin)/(stride*sizeof(float));
      if(offset<=reference_vertex_count && count<=reference_vertex_count-offset)refs=reference_vertices+offset;
    }
  }
  auto vertex=[&](size_t i){return pica_vertex_at(buf,stride,i,refs);};
  if(reticle_scope && stereo_active && count){
   float x0=1.e9f,x1=-1.e9f,y0=1.e9f,y1=-1.e9f;
   for(size_t i=0;i<count;i++){const float *v=vertex(i);if(v[3]>0){
    x0=std::min(x0,v[0]/v[3]);x1=std::max(x1,v[0]/v[3]);
    y0=std::min(y0,v[1]/v[3]);y1=std::max(y1,v[1]/v[3]);
   }}
   if(x0<=x1 && y0<=y1){
    reticle_x=last_viewport[0]+((x0+x1)*.25f+.5f)*last_viewport[2];
    reticle_y=last_viewport[1]+(.5f-(y0+y1)*.25f)*last_viewport[3];reticle_seen=true;
   }
  }
  const unsigned channels = c.opt_alpha ? 4 : 3;
  const size_t input_offset = stride - channels * c.num_inputs;
  float fog_color[3] = {0, 0, 0};
  const size_t fog_offset = input_offset - (c.opt_fog ? 4 : 0);
  const uint64_t *draw_ids=nullptr;
  static const bool reuse_enabled=getenv("EOG_DISABLE_VERTEX_REUSE")==nullptr;
  // Split draws keep the original producer base. Integer range checks avoid
  // subtracting unrelated pointers for callers that do not provide IDs.
  if(reuse_enabled && source_vertex_ids && source_vertex_base) {
    uintptr_t begin=reinterpret_cast<uintptr_t>(source_vertex_base),at=reinterpret_cast<uintptr_t>(buf);
    if(at>=begin && (at-begin)%(stride*sizeof(float))==0) {
      size_t offset=(at-begin)/(stride*sizeof(float));
      if(offset<=source_vertex_count && count<=source_vertex_count-offset)draw_ids=source_vertex_ids+offset;
    }
  }
  static const bool indexed_enabled=getenv("EOG_DISABLE_GPU_INDICES")==nullptr;
  // CPU background rasterization regressed on hardware with indexed draws.
  // Keep the original ordered array path for CPU clip coordinates; sharing
  // remains enabled only for model-space GPU submissions.
  bool indexed=indexed_enabled && gpu_world && !watch_scope && !c.opt_noise && draw_ids && world_indices &&
      world_index_count+count<=MAX_VERTICES && !(sharp_depth.test && sharp_depth.bias>0);
  size_t first_index=world_index_count,unique_count=0;
  bool cacheable=mesh_cache_enabled && gpu_world && !world_palette_active && !refs && !indexed && !watch_scope && !c.opt_noise &&
      !(sharp_depth.test && sharp_depth.bias>0);
  PicaMeshCache::Key mesh_key={active->id0,active->id1,unsigned(count),unsigned(stride),c.opt_fog?unsigned(fog_offset):~0u,{}};
  bool retainable=retained_token && indexed && gpu_world;
  if(cacheable || retainable)for(unsigned t=0;t<2;t++)if(c.used_textures[t]) {
    auto *tex=texture(t);if(!tex->ready){cacheable=false;retainable=false;}
    mesh_key.dimensions[t*4]=tex->width;mesh_key.dimensions[t*4+1]=tex->height;
    mesh_key.dimensions[t*4+2]=tex->gpu.width;mesh_key.dimensions[t*4+3]=tex->gpu.height;
  }
  unsigned lod_at=~0u;PicaInputPlan lod_plan={};
  bool camera_lod=gpu_world && !refs && !indexed && pica_plan_lod_inputs(buf,stride,input_offset,count,c.num_inputs,c.opt_alpha,
      lod_rgb_hint,lod_alpha_hint,&lod_plan,&lod_at) &&
      pica_lod_route_supported(c,lod_plan,buf,input_offset,c.opt_alpha);
  if(camera_lod){mesh_key.lod=lod_at;mesh_key.lod_alpha=c.opt_alpha && lod_alpha_hint;}
  uint32_t mesh_hash=0;
  auto *cached=cacheable?mesh_cache.find(mesh_key,buf,mesh_hash):nullptr;
  if(camera_lod){mesh_camera_lod_draws++;if(cached)mesh_camera_lod_hits++;}
  PicaInputPlan proven_plan={};
  bool proven=(!gpu_world || refs || indexed) && c.num_inputs>=2 && (uniform_input_hint || lod_rgb_hint) &&
    pica_proven_inputs(c,vertex(0),input_offset,uniform_input_hint,alpha_uniform_hint,lod_rgb_hint,lod_alpha_hint,&proven_plan);
  if(c.num_inputs>=2){if(proven)proven_input_draws++;else scanned_input_draws++;}
  const u64 planning_start=measure_frame?svcGetSystemTick():0;
  bool mapped=false,alpha_product=false;
  if(cached) {
    inputs=cached->inputs;alpha_product=cached->alpha_product;
    memcpy(fog_color,cached->fog_colour,sizeof fog_color);mapped=true;
  }else {
    if(proven)inputs=proven_plan;else if(camera_lod)inputs=lod_plan;
    const bool colours_mapped=proven || camera_lod || pica_plan_inputs_view(buf,stride,input_offset,count,c.num_inputs,c.opt_alpha,&inputs,refs);
    const bool fog_mapped=!c.opt_fog || plan_batch_fog(buf,stride,fog_offset,count,fog_color,refs);
    mapped=colours_mapped && fog_mapped;
    if(!mapped && fog_mapped)
      mapped=alpha_product=pica_alpha_product_plan(c,buf,stride,input_offset,count,&inputs,refs);
  }
  if(measure_frame)planning_ticks+=svcGetSystemTick()-planning_start;
  if(!mapped) {
    if(triangles>1) {
      size_t consumed=0;
      while(consumed<triangles) {
        float *part=buf+consumed*3*stride;
        const u64 split_start=measure_frame?svcGetSystemTick():0;
        size_t span=pica_batch_prefix(part,stride,input_offset,triangles-consumed,c.num_inputs,c.opt_alpha,c.opt_fog,refs?refs+consumed*3:nullptr);
        if(measure_frame)planning_ticks+=svcGetSystemTick()-split_start;
        if(!span)span=1; // Single-triangle validation below reports its inputs.
        draw(part,span*3*stride,span);consumed+=span;
      }
      return;
    }
    for(unsigned input=0;input<(unsigned)c.num_inputs;++input)
      for(unsigned v=0;v<3;++v) {
        const float *p=vertex(v)+input_offset+input*channels;
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
    memcpy(constant, vertex(0) + input_offset + inputs.constant * channels,
           channels * sizeof(float));
  if (!active->colors_logged) {
    active->colors_logged = true;
    sysLogPrintf(2,"PICA color plan %llx/%x: primary=%d constant=%d scalar=%d fogRGB=%.3f,%.3f,%.3f",
                 (unsigned long long)active->id0,active->id1,inputs.primary,
                 inputs.constant,inputs.scalar,(double)fog_color[0],
                 (double)fog_color[1],(double)fog_color[2]);
    for (unsigned input=0; input<(unsigned)c.num_inputs; ++input) {
      const float *sample=vertex(0)+input_offset+input*channels;
      sysLogPrintf(2,"  input%u RGBA=%.3f,%.3f,%.3f,%.3f",input,
                   (double)sample[0],(double)sample[1],(double)sample[2],
                   c.opt_alpha?(double)sample[3]:1.0);
    }
    if(c.opt_fog) {
      float low=1.f,high=0.f;
      for(size_t v=0;v<count;++v) {
        float factor=vertex(v)[fog_offset+3];
        low=std::min(low,factor); high=std::max(high,factor);
      }
      sysLogPrintf(2,"  fog factor range=%.3f..%.3f",(double)low,(double)high);
    }
  }
  // Bind/validate textures and their padded dimensions once per batch.
  // GPU dimensions are powers of two, so multiplication by their exact
  // reciprocal preserves the old division result without per-vertex divides.
  Texture *draw_textures[2]={nullptr,nullptr};
  float inv_width[2]={},inv_height[2]={};
  for(unsigned t=0;t<2;t++)if(c.used_textures[t]) {
    draw_textures[t]=texture(t);
    if(!draw_textures[t]->ready)sysFatalError("Native PICA: uninitialized texture");
    ensure_texture_lods(draw_textures[t]);
    unsigned forced=std::min(texture_lod_level,draw_textures[t]->levels);
    unsigned maximum=forced?forced:draw_textures[t]->requested_mips && mip_filter!=MIPMAP_DISABLED?draw_textures[t]->levels:0;
    if(draw_textures[t]->gpu.minLevel!=forced || draw_textures[t]->gpu.maxLevel!=maximum) {flush_draw();apply_sampler(draw_textures[t]);}
    inv_width[t]=1.f/draw_textures[t]->gpu.width;
    inv_height[t]=1.f/draw_textures[t]->gpu.height;
  }
  ++texture_lod_draws[texture_lod_level];
  const uint64_t *attribute_ids=nullptr;
  static const bool attribute_reuse=getenv("EOG_DISABLE_STATIC_ATTRIBUTES")==nullptr;
  if(attribute_reuse && (!gpu_world || refs) && !watch_scope && !c.opt_noise && !alpha_product && attribute_source_ids && attribute_source_base) {
    uintptr_t begin=reinterpret_cast<uintptr_t>(attribute_source_base),at=reinterpret_cast<uintptr_t>(buf);
    if(at>=begin && (at-begin)%(stride*sizeof(float))==0) {
      size_t offset=(at-begin)/(stride*sizeof(float));
      if(offset<=attribute_source_count && count<=attribute_source_count-offset)attribute_ids=attribute_source_ids+offset;
    }
  }
  uint32_t attribute_context=0;
  if(attribute_ids) {
    PicaStaticAttributes<Vertex>::Context key;memset(&key,0,sizeof key);
    key.shader=active->id0;key.flags=active->id1;
    int plan[]={inputs.primary,inputs.constant,inputs.scalar,inputs.scalar_alpha,inputs.primary_alpha,inputs.primary_fixed_alpha,int(inputs.zero_third)};
    for(unsigned i=0;i<7;i++)key.plan[i]=plan[i];
    key.lod_rgb=lod_rgb_hint;key.lod_alpha=lod_alpha_hint;
    for(unsigned t=0;t<2;t++)if(draw_textures[t]) {
      auto *tex=draw_textures[t];key.dimensions[t*4]=tex->width;key.dimensions[t*4+1]=tex->height;
      key.dimensions[t*4+2]=tex->gpu.width;key.dimensions[t*4+3]=tex->gpu.height;
    }
    attribute_context=static_attributes.context(key);
  }
  PicaRetainedMesh::Key retained_key={retained_token,draw_ids?unsigned(draw_ids-source_vertex_ids):0,mesh_key,inputs,alpha_product};
  retainable=retainable && !alpha_product;
  auto *retained=retainable?retained_meshes.find(retained_key,retained_stream_enabled()):nullptr;
  if(retained) {
    for(unsigned i=0;i<retained->vertices;i++) {
      const float *p=vertex(retained->corners[i]);auto &v=retained->gpu[i];
      v.pos[3]=float(world_palette_row);
      if(c.opt_fog)v.uv[2][1]=pica_lookup_t(pica_ramp_coordinate(p[fog_offset+3]));
      const float *col=p+input_offset;
      if(c.num_inputs)memcpy(v.color,col+inputs.primary*channels,channels*sizeof(float));
      if(c.opt_alpha) {
        int alpha=inputs.primary_alpha>=0?inputs.primary_alpha:inputs.primary;
        if(alpha>=0)v.color[3]=col[alpha*channels+3];
      }
      if(inputs.scalar>=0)v.uv[2][0]=pica_ramp_coordinate(col[inputs.scalar*channels]);
    }
    retained_meshes.dirty(*retained);
  }
  if(!cached && !retained)for (size_t i = 0; i < count; ++i) {
    const float *p = vertex(i);
    Vertex &v = vertices[vertex_count + (indexed?unique_count:i)];
    uint64_t source_id=draw_ids?draw_ids[i]:0;
    ConvertedVertexSlot *slot=source_id?&converted_slots[(source_id-1)&127]:nullptr;
    if(slot && slot->id==source_id && slot->index>=vertex_count && slot->index<vertex_count+(indexed?unique_count:i)) {
      if(indexed){world_indices[first_index+i]=slot->index;indexed_saved++;}
      else v=vertices[slot->index];
      ++converted_reused;continue;
    }
    size_t output_vertex=vertex_count+(indexed?unique_count++:i);
    if(indexed)world_indices[first_index+i]=output_vertex;
    if(slot){slot->id=source_id;slot->index=output_vertex;}
    ++converted_built;
    uint64_t attribute_id=attribute_ids?attribute_ids[i]:0;
    if(attribute_id && static_attributes.read(attribute_id,attribute_context,v)) {
      memcpy(v.pos,p,16);
      if(world_palette_active)v.pos[3]=float(world_palette_row);
      else if(!gpu_world)v.pos[2]=(p[2]-p[3])*.5f;
      if(c.opt_fog)v.uv[2][1]=pica_lookup_t(pica_ramp_coordinate(p[fog_offset+3]));
      const float *col=p+input_offset;
      if(lod_rgb_hint&(1u<<inputs.primary))memcpy(v.color,col+inputs.primary*channels,3*sizeof(float));
      if(c.opt_alpha) {
        int alpha=inputs.primary_alpha>=0?inputs.primary_alpha:inputs.primary;
        if(lod_alpha_hint&(1u<<alpha))v.color[3]=col[alpha*channels+3];
      }
      if(inputs.scalar>=0 && (lod_rgb_hint&(1u<<inputs.scalar)))v.uv[2][0]=pica_ramp_coordinate(col[inputs.scalar*channels]);
      continue;
    }
    memcpy(v.pos, p, 16);
    if(world_palette_active)v.pos[3]=float(world_palette_row);
    else if(!gpu_world)v.pos[2]=(p[2]-p[3])*.5f;
    p += 4;
    for (unsigned t = 0; t < 3; ++t)
      v.uv[t][0] = v.uv[t][1] = 0;
    for (unsigned t = 0; t < 2; ++t)
      if (c.used_textures[t]) {
        float u = *p++, vv = *p++;
        if (c.clamp[t][0])
          u = std::clamp(u, 0.f, *p++);
        if (c.clamp[t][1])
          vv = std::clamp(vv, 0.f, *p++);
        Texture *tex=draw_textures[t];
        v.uv[t][0] = pica_texture_scaled(u,tex->width,inv_width[t]);
        v.uv[t][1] = 1.f-pica_texture_scaled(vv,tex->height,inv_height[t]);
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
    if(attribute_id)static_attributes.write(attribute_id,attribute_context,v);
  }
  if(cpu_world_hint && !gpu_world)++cpu_array_draws;
  // Stage cached model parts into the common immutable frame stream. This
  // lets adjacent compatible bones share one indexed GPU draw while each
  // vertex retains its own palette selector and live shade/fog values.
  bool streamed=retained_stream_enabled() && retainable;
  bool streamed_hit=streamed && retained;
  if(streamed && retained) {
    unique_count=retained->vertices;
    memcpy(vertices+vertex_count,retained->gpu,unique_count*sizeof(Vertex));
    for(size_t i=0;i<count;i++)world_indices[first_index+i]=uint16_t(vertex_count+retained->indices[i]);
    retained=nullptr;++retained_streamed_parts;
  }
  size_t written_count=retained?0:indexed?unique_count:count;
  if(indexed && !retained){world_index_count+=count;indexed_draws++;}
  if(retainable && !retained && !streamed_hit)retained=retained_meshes.store(retained_key,vertices+vertex_count,unique_count,world_indices+first_index,vertex_count);
  if(streamed && retained){retained=nullptr;++retained_streamed_parts;}
  if(cacheable && !cached)cached=mesh_cache.store(mesh_key,mesh_hash,buf,vertices+vertex_count,inputs,alpha_product,fog_color);
  ++draw_calls;if(gpu_world)++gpu_world_draws;
  if(!gpu_world && !retained && frame_count%120==0)for(size_t i=0;i<written_count;++i) {
    const Vertex &v=vertices[vertex_count+i];
    if(v.color[0]>.1f || v.color[1]>.1f || v.color[2]>.1f)++bright_vertices;
    near_vertices+=v.pos[2]<-v.pos[3];far_vertices+=v.pos[2]>0;behind_vertices+=v.pos[3]<=0;
    if(v.pos[3]>0 && std::fabs(v.pos[0])<=v.pos[3] && std::fabs(v.pos[1])<=v.pos[3] && v.pos[2]>=-v.pos[3] && v.pos[2]<=0)++clip_vertices;
  }
  PicaTevKey key={active->id0,active->id1,inputs,
                 pica_constant_kind(2,0,constant),pica_constant_kind(2,1,constant)};
  MaterialSetup *setup=material_cache.find(key);
  if(setup)++material_hits;
  else {
    MaterialSetup built={};++material_builds;
    bool signed_alpha=signed_debris_alpha(constant);
    built.alpha[2]=99;
    for(unsigned i=0;i<6;++i)C3D_TexEnvInit(&built.env[i]);
    for(unsigned cycle=0;cycle<(c.opt_2cyc?2u:1u);++cycle) {
      C3D_TexEnv *env=&built.env[cycle];
      built.rgb[cycle]=built.alpha[cycle]=99;
      combiner_channel(env,cycle,0,constant,built.rgb[cycle]);
      if(c.opt_alpha && !signed_alpha)combiner_channel(env,cycle,1,constant,built.alpha[cycle]);
      else {
        C3D_TexEnvSrc(env,C3D_Alpha,GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env,C3D_Alpha,GPU_REPLACE);
      }
    }
    if(c.opt_fog) {
      C3D_TexEnv *env=&built.env[c.opt_2cyc?2:1];
      C3D_TexEnvSrc(env,C3D_RGB,GPU_CONSTANT,GPU_PREVIOUS,GPU_TEXTURE2);
      C3D_TexEnvOpRgb(env,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_ALPHA);
      C3D_TexEnvFunc(env,C3D_RGB,GPU_INTERPOLATE);
      C3D_TexEnvSrc(env,C3D_Alpha,GPU_PREVIOUS);
      C3D_TexEnvFunc(env,C3D_Alpha,GPU_REPLACE);
    }
    if(signed_alpha)configure_signed_debris(built,constant);
    material_cache.put(key,built);setup=material_cache.find(key);
  }
  if(sharp_depth.test && sharp_depth.bias>0)for(size_t i=0;i+2<count;i+=3) {
    Vertex *tri=vertices+vertex_count+i;
    float offset=pica_decal_slope(tri[0].pos,tri[1].pos,tri[2].pos,last_viewport[2],last_viewport[3],true);
    for(unsigned j=0;j<3;j++)tri[j].pos[2]-=offset*tri[j].pos[3];
  }
  PicaDrawState state={};state.tev=key;
  for(unsigned cycle=0;cycle<(c.opt_2cyc?2u:1u);++cycle) {
    unsigned kind=setup->rgb[cycle],ak=setup->alpha[cycle];
    unsigned rgb=kind==2?byte(constant[0])|(byte(constant[1])<<8)|(byte(constant[2])<<16):kind==1?0xffffff:0;
    unsigned a=ak==2?byte(constant[3]):ak==1?255:0;
    state.colours[cycle]=rgb|(a<<24);
  }
  if(c.opt_fog)state.colours[2]=byte(fog_color[0])|(byte(fog_color[1])<<8)|(byte(fog_color[2])<<16);
  if(setup->signed_alpha) {
    unsigned kind=setup->alpha[2];
    state.colours[2]|=(kind==2?byte(constant[3]):kind==1?255u:0u)<<24;
  }
  for(unsigned t=0;t<2;t++)state.textures[t]=c.used_textures[t]?selected[t]:0;
  if(!cached && !retained && pending_indexed==indexed && pending_draw.extendable(state,indexed?first_index:vertex_count)) {
    pending_draw.count+=count;++merged_draws;
    vertex_count+=written_count;triangle_count+=triangles;return;
  }
  flush_draw();
  if(!c.opt_noise && tev_valid && pica_tev_same(key,tev_key))++tev_reuses;
  else {
    ++tev_configs;C3D_TexEnvBufUpdate(C3D_Both,0);
    if(setup->signed_alpha)C3D_TexEnvBufUpdate(C3D_Alpha,1u<<1);
    for(unsigned i=0;i<6;i++)C3D_SetTexEnv(i,&setup->env[i]);
    tev_key=key;tev_valid=!c.opt_noise;
  }
  for(unsigned cycle=0;cycle<(c.opt_2cyc?2u:1u);++cycle)
    C3D_TexEnvColor(C3D_GetTexEnv(cycle),state.colours[cycle]);
  if(c.opt_fog)C3D_TexEnvColor(C3D_GetTexEnv(c.opt_2cyc?2:1),state.colours[2]);
  else if(setup->signed_alpha)C3D_TexEnvColor(C3D_GetTexEnv(2),state.colours[2]);
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
  if(retained) {
    C3D_SetBufInfo(&retained->buffer);pending_draw.begin(state,0,count);
    pending_indexed=true;pending_elements=retained->indices;flush_draw();C3D_SetBufInfo(&top_buffer);
  }else if(cached) {
    // Prior draws were flushed above before changing the GPU buffer base.
    C3D_SetBufInfo(&cached->buffer);pending_draw.begin(state,0,count);flush_draw();
    C3D_SetBufInfo(&top_buffer);
  }else {pending_draw.begin(state,indexed?first_index:vertex_count,count);pending_indexed=indexed;}
  vertex_count += written_count;
  triangle_count += triangles;
}
/* This operation is always outside a C3D frame. Output replacement waits
 * for the previous GPU transfer before any old framebuffer is freed. Keep
 * both old eyes intact until both new allocations have succeeded. */
static void invalidate_stereo_depth(){depth_frame.usable=false;auto_convergence.reset();aim_depth.reset();world_convergence=native_stereo_options::convergence(native_setting_stereo_convergence());reticle_shift=0;}
static void discard_stereo_snapshot(){depth_frame.usable=false;}
static bool configure_top_samples(int scale) {
  discard_stereo_snapshot();
  C3D_RenderTarget *left=C3D_RenderTargetCreate(240*scale,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);
  if(!left)return false;
  C3D_RenderTarget *right=C3D_RenderTargetCreate(240*scale,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);
  if(!right){C3D_RenderTargetDelete(left);return false;}
  const unsigned flags=top_output_flags|GX_TRANSFER_SCALING(scale==2?GX_TRANSFER_SCALE_X:GX_TRANSFER_SCALE_NO);
  C3D_RenderTargetSetOutput(left,GFX_TOP,GFX_LEFT,flags);
  C3D_RenderTargetSetOutput(right,GFX_TOP,GFX_RIGHT,flags);
  if(target)C3D_RenderTargetDelete(target);
  if(right_target)C3D_RenderTargetDelete(right_target);
  target=left;right_target=right;top_sample_scale=scale;
  sysLogPrintf(2,"Top AA: %s; each eye renders %dx%d -> 400x240",scale==2?"3DS 2x hardware downsample":"off",400,240*scale);
  return true;
}
#include "pica_sharpness.h"
static void update_sharp_targets(int samples,int strength) {
  post_vignette=native_setting_vignette();post_vignette_mode=native_setting_vignette_mode();post_bloom=native_setting_bloom();post_grain=native_setting_grain();
  bool wanted=strength || post_vignette || post_bloom || post_grain || native_stereo_options::ghost(native_setting_stereo_ghost_contrast(),native_setting_stereo_black_floor()).active;
  if(wanted==sharp_enabled && samples==top_sample_scale){sharp_strength=strength;return;}
  if(!wanted && !sharp_enabled)return; // AA-only path remains unchanged.
  // Rebuild the small watch target last too. Keeping it in the middle of
  // either VRAM bank leaves holes too small for a padded scene surface.
  watch_end_output();
  if(watch_target){C3D_RenderTargetDelete(watch_target);watch_target=nullptr;}
  sharp_release_targets();
  if(wanted && sharp_create_targets(samples)) {
    watch_target=C3D_RenderTargetCreate(240,320,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);
    if(watch_target) {
    sharp_strength=strength;sysLogPrintf(2,"Sharpness enabled: strength=%d samples=%d",strength,samples);return;
    }
  }
  sharp_release_targets();native_setting_set_stereo_ghost_contrast(100);native_setting_set_stereo_black_floor(0);native_setting_set_sharpness(0);native_setting_set_vignette(0);native_setting_set_bloom(0);native_setting_set_grain(0);sharp_strength=post_vignette=post_bloom=post_grain=0;
  if(!configure_top_samples(samples)) {
    if(samples==1 || !configure_top_samples(1))sysFatalError("Native PICA: cannot restore screen targets");
    native_setting_set_aa(0);
  }
  watch_target=C3D_RenderTargetCreate(240,320,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);
  if(!watch_target)sysFatalError("Native PICA: cannot restore watch target");
  sysLogPrintf(2,"Sharpness off: normal output restored%s",strength?" (insufficient VRAM for filter)":"");
}
/* Flush and queue a tiled depth copy before hands/HUD can overwrite it.
 * The next normal FrameBegin completes it; no extra wait or GPU readback fence. */
static void queue_stereo_depth(){
 if((!native_setting_stereo_auto() && native_setting_stereo_crosshair_hud()) || depth_copied || !stereo_active || frontend_ui || !depth_frame.usable || !depth_copy)return;
 unsigned bytes=depth_frame.width*depth_frame.height*4;
 if(!bytes || bytes>DEPTH_COPY_CAPACITY){depth_frame.usable=false;return;}
 mesh_cache.flush();retained_meshes.flush();
 GSPGPU_FlushDataCache(vertices,vertex_count*sizeof(Vertex));
 if(world_index_count)GSPGPU_FlushDataCache(world_indices,world_index_count*sizeof(uint16_t));
 C3D_FrameSplit(0);
 unsigned dim=GX_BUFFER_DIM(240*8*2/16,0);
 if(R_FAILED(GX_TextureCopy(static_cast<u32 *>(depth_frame.target->frameBuf.depthBuf),dim,depth_copy,dim,bytes,BIT(3)))){
  depth_frame.usable=false;return;
 }
 depth_frame.pixels=depth_copy;depth_frame.bytes=bytes;depth_copied=true;
}
static uint32_t read_stereo_packed(const StereoDepthFrame &f,float x,float y){
 if(!f.target || !f.width || !f.height || x<0 || x>1 || y<0 || y>1)return 0;
 auto *buffer=f.pixels?f.pixels:static_cast<const uint32_t *>(f.target->frameBuf.depthBuf);
 if(!buffer)return 0;
 float nx=x*2-1,ny=1-y*2;
 const auto &r0=f.tilt.r[0],&r1=f.tilt.r[1];
 float rx=r0.x*nx+r0.y*ny+r0.w,ry=r1.x*nx+r1.y*ny+r1.w;
 int px=int(f.viewport[1]*f.samples+(rx*.5f+.5f)*f.viewport[3]*f.samples);
 int py=int(400-f.viewport[0]-f.viewport[2]+(ry*.5f+.5f)*f.viewport[2]);
 py=f.height-1-py;
 if(px<0 || py<0 || unsigned(px)>=f.width || unsigned(py)>=f.height)return 0;
 return buffer[native_stereo::tiled_index(px,py,f.width)];
}
static float read_stereo_depth(const StereoDepthFrame &f,float x,float y){
 return f.projection.distance(read_stereo_packed(f,x,y),f.scale,f.bias);
}
static void update_stereo_depth(float seconds){
 const int automatic=native_setting_stereo_auto(),manual_percent=native_setting_stereo_convergence(),comfort=native_setting_stereo_comfort();
 static int last_auto=-1,last_manual=-1,last_comfort=999;
 if(automatic!=last_auto || manual_percent!=last_manual || comfort!=last_comfort){
  invalidate_stereo_depth();last_auto=automatic;last_manual=manual_percent;last_comfort=comfort;
 }
 const float manual=native_stereo_options::convergence(manual_percent);
 if(!automatic)world_convergence=manual;
 const bool dynamic_reticle=!native_setting_stereo_crosshair_hud();
 if(!stereo_active || frontend_ui || !target){invalidate_stereo_depth();return;}
 if(watch_seen){depth_frame.usable=false;aim_depth.reset();reticle_shift=0;return;}
 // A missing world frame preserves convergence/aim history as in rt64-3ds.
 if(!reticle_seen || !dynamic_reticle)aim_depth.reset();
 if(!depth_frame.usable || depth_frame.target!=target || !depth_copied || !depth_frame.pixels){
  reticle_shift=reticle_seen && dynamic_reticle?aim_depth.shift(world_convergence,stereo_slider):0;return;
 }
 auto &f=depth_frame;
 if(R_FAILED(GSPGPU_InvalidateDataCache(f.pixels,f.bytes))){depth_frame.usable=false;return;}
 auto canonical=[&](float x,float y){return f.projection.canonical(read_stereo_packed(f,x,y),f.scale,f.bias);};
 if(automatic){
  float zp=native_stereo::near_depth(canonical,f.viewport[2],f.viewport[3]);
  float near=f.projection.distance_from_canonical(zp);
  world_convergence=auto_convergence.update(near,stereo_slider,seconds,f.projection.near_plane(),manual,comfort);
 }
 if(reticle_seen && dynamic_reticle){
  auto read=[&](float x,float y){return read_stereo_depth(f,(x-f.viewport[0])/f.viewport[2],(y-f.viewport[1])/f.viewport[3]);};
  aim_depth.sample(read,reticle_x,reticle_y,float(f.viewport[2]),f.slider,f.convergence,seconds);
 }
 reticle_shift=reticle_seen && dynamic_reticle?aim_depth.shift(world_convergence,stereo_slider):0;
 depth_frame.usable=false;
}

static void init() {

  top_output_flags=native_display_prepare();
  if (!C3D_Init(0x80000))sysFatalError("Native PICA: initialization failed");
  if(!configure_top_samples(native_setting_aa()?2:1)) {
    if(!native_setting_aa() || !configure_top_samples(1))sysFatalError("Native PICA: render target allocation failed");
    native_setting_set_aa(0);sysLogPrintf(2,"AA disabled: insufficient framebuffer memory");
  }
  watch_target=C3D_RenderTargetCreate(240,320,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);
  if(!watch_target)sysFatalError("Native PICA: watch output allocation failed");
  C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE,
                 GPU_ZERO);
  shader = DVLB_ParseFile((u32 *)native_clip_shbin, native_clip_shbin_size);
  if (!shader)
    sysFatalError("Native PICA: invalid shader");
  shaderProgramInit(&program);
  shaderProgramSetVsh(&program, &shader->DVLE[0]);
  tilt_uniform=shaderInstanceGetUniformLocation(program.vertexShader,"tilt");
  if(tilt_uniform<0)sysFatalError("Native PICA: vertex uniforms missing");
  clip_tilt_uniform=tilt_uniform;
  clip_stereo_uniform=shaderInstanceGetUniformLocation(program.vertexShader,"stereo");
  if(clip_stereo_uniform<0)sysFatalError("Native PICA: stereo uniform missing");
  world_shader=DVLB_ParseFile((u32 *)native_world_shbin,native_world_shbin_size);
  if(!world_shader)sysFatalError("Native PICA: invalid world shader");
  shaderProgramInit(&world_program);shaderProgramSetVsh(&world_program,&world_shader->DVLE[0]);
  world_tilt_uniform=shaderInstanceGetUniformLocation(world_program.vertexShader,"tilt");
  world_stereo_uniform=shaderInstanceGetUniformLocation(world_program.vertexShader,"stereo");
  if(world_stereo_uniform<0)sysFatalError("Native PICA: world stereo uniform missing");
  world_matrix_uniform=shaderInstanceGetUniformLocation(world_program.vertexShader,"world");
  if(world_tilt_uniform<0 || world_matrix_uniform<0 || world_matrix_uniform+WORLD_PALETTE_SIZE*4>96)
    sysFatalError("Native PICA: world uniforms missing/overflow");
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
  world_indices=static_cast<uint16_t *>(linearAlloc(MAX_VERTICES*sizeof(uint16_t)));
  watch_vertices=static_cast<Vertex *>(linearAlloc(MAX_VERTICES*sizeof(Vertex)));
  if(!watch_vertices)sysFatalError("Native PICA: watch vertex allocation failed");
  BufInfo_Init(&watch_buffer);BufInfo_Add(&watch_buffer,watch_vertices,sizeof(Vertex),5,0x43210);
}
static void resize() {}
static void start() {
  // Detailed counters are only printed every 120 frames. Do not pay four
  // kernel clock reads per draw on the other 119 frames.
  measure_frame=frame_count%120==0;
  texture_lod_level=0;texture_lod_draws[0]=texture_lod_draws[1]=texture_lod_draws[2]=texture_lod_draws[3]=0;texture_lod_promotions=0;
  if(measure_frame && native_platform_log_batch_begin)native_platform_log_batch_begin();
  // Reallocate only at frame boundaries, before C3D_FrameBegin. Watch/menu
  // changes are atomically sampled once here, never during an active draw.
  int requested_samples=native_setting_aa()?2:1;
  if(requested_samples!=top_sample_scale || bool(native_setting_sharpness() || native_setting_vignette() || native_setting_bloom() || native_setting_grain() || native_stereo_options::ghost(native_setting_stereo_ghost_contrast(),native_setting_stereo_black_floor()).active)!=sharp_enabled)discard_stereo_snapshot();
  update_sharp_targets(requested_samples,native_setting_sharpness());
  requested_samples=native_setting_aa()?2:1;
  if(!sharp_enabled && requested_samples!=top_sample_scale && !configure_top_samples(requested_samples)) {
    native_setting_set_aa(top_sample_scale==2);
    sysLogPrintf(2,"AA change cancelled: insufficient framebuffer memory; old output retained");
  }
  gpu_wait_start=svcGetSystemTick();
  tev_valid=false;tev_configs=tev_reuses=0;
  material_hits=material_builds=gpu_draws=merged_draws=0;pending_draw.clear();retained_streamed_parts=0;
  __atomic_store_n(&gpu_phase,1,__ATOMIC_RELEASE);
  uint64_t native_gpu_start=native_work_profile_begin();
  if (!C3D_FrameBegin(0))
    sysFatalError("Native PICA: frame begin failed");
  native_work_profile_end(NATIVE_WORK_GPU_WAIT,native_gpu_start);
  // Charge depth sampling to CPU render time, not the preceding GPU wait.
  cpu_frame_start=svcGetSystemTick();
  gpu_wait_ms=(double)(cpu_frame_start-gpu_wait_start)*1000.0/SYSCLOCK_ARM11;
  stereo_slider=osGet3DSliderState()*native_setting_stereo_strength()/100.f;stereo_active=stereo_slider>0.f;
  static u64 stereo_last_tick;
  u64 stereo_now=svcGetSystemTick();
  float stereo_dt=stereo_last_tick?float(stereo_now-stereo_last_tick)/SYSCLOCK_ARM11:1.f/60.f;
  stereo_last_tick=stereo_now;
  update_stereo_depth(stereo_dt);
  if(stereo_active && !frontend_ui && (native_setting_stereo_auto() || !native_setting_stereo_crosshair_hud()) && !depth_copy && !depth_copy_failed){
   depth_copy=static_cast<uint32_t *>(linearAlloc(DEPTH_COPY_CAPACITY));
   depth_copy_failed=depth_copy==nullptr;
   if(depth_copy_failed)sysLogPrintf(2,"Adaptive stereo disabled: no depth snapshot memory");
  }
  depth_frame={};depth_copied=false;reticle_seen=reticle_scope=sky_scope=false;stereo_projection={};

  mesh_cache.begin_frame();mesh_camera_lod_draws=mesh_camera_lod_hits=0;proven_input_draws=scanned_input_draws=0;
  retained_meshes.begin_frame();retained_token=0;pending_elements=nullptr;
  draw_ticks=upload_ticks=planning_ticks=0;
  if(gpu_wait_ms>100.0)sysLogPrintf(2,"GPU wait hitch: %.2fms frame=%u",gpu_wait_ms,frame_count);
  __atomic_store_n(&gpu_phase,2,__ATOMIC_RELEASE);
  // Blocking FrameBegin(0) still waits for prior GPU commands before reuse.
  // The original VI scheduler paces frames; do not add FrameSync/vblank here.
  for (auto &t : retired)
    C3D_TexDelete(&t);
  retired.clear();
  stereo_slider=osGet3DSliderState()*native_setting_stereo_strength()/100.f;stereo_active=stereo_slider>0.f;
  ui_rect_scope=false;
  if(stereo_active)C3D_RenderTargetClear(right_target,C3D_CLEAR_ALL,0x000000ff,0);
  C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0x000000ff, 0);
  in_frame = true;
  static_attributes.begin_frame();
  memset(converted_slots,0,sizeof converted_slots);converted_reused=converted_built=0;
  world_index_count=0;pending_indexed=false;indexed_draws=indexed_saved=cpu_array_draws=0;cpu_world_hint=false;
  vertex_count = 0;viewmodel_scope=false;watch_scope=watch_tabs=watch_body=watch_seen=watch_rect_active=false;watch_canvas_scale=1.f;watch_canvas_offset=0;
  bright_vertices=clip_vertices=draw_calls=near_vertices=far_vertices=behind_vertices=0;
  C3D_FrameDrawOn(target);
  C3D_SetViewport(0,0,240*top_sample_scale,400);
  gpu_world=world_palette_active=false;world_palette_count=world_palette_row=0;
  world_palette_switches=world_palette_continued=world_palette_rollovers=0;
  gpu_world_draws=0;tilt_uniform=clip_tilt_uniform;
  C3D_BindProgram(&program);
  C3D_Mtx tilt;
  Mtx_OrthoTilt(&tilt, -1, 1, -1, 1, 0, 1, true);
  stereo_tilt[0]=tilt;
  const float disparity=native_stereo_options::hud_pixels(native_setting_stereo_hud_depth())*stereo_slider/400.f;
  Mtx_OrthoTilt(&stereo_tilt[1],-1-disparity,1-disparity,-1,1,0,1,true);
  Mtx_OrthoTilt(&stereo_tilt[2],-1+disparity,1+disparity,-1,1,0,1,true);
  const float slider=stereo_slider;
  native_weapon_stereo(stereo_tilt[3],tilt,slider,false);
  native_weapon_stereo(stereo_tilt[4],tilt,slider,true);
  native_world_stereo(stereo_tilt[5],tilt,slider,false);
  native_world_stereo(stereo_tilt[6],tilt,slider,true);
  native_depth_stereo(stereo_tilt[7],tilt,1.f,false,-reticle_shift*200.f);
  native_depth_stereo(stereo_tilt[8],tilt,1.f,true,-reticle_shift*200.f);
  if(native_setting_stereo_crosshair_hud()){stereo_tilt[7]=stereo_tilt[1];stereo_tilt[8]=stereo_tilt[2];}
  native_sky_stereo(stereo_tilt[9],tilt,slider,false);
  native_sky_stereo(stereo_tilt[10],tilt,slider,true);
  C3D_FVUnifMtx4x4(
      GPU_VERTEX_SHADER,
      tilt_uniform, &tilt);
  C3D_AttrInfo *a = C3D_GetAttrInfo();
  AttrInfo_Init(a);
  AttrInfo_AddLoader(a, 0, GPU_FLOAT, 4);
  AttrInfo_AddLoader(a, 1, GPU_FLOAT, 4);
  AttrInfo_AddLoader(a, 2, GPU_FLOAT, 2);
  AttrInfo_AddLoader(a, 3, GPU_FLOAT, 2);
  AttrInfo_AddLoader(a, 4, GPU_FLOAT, 2);
  C3D_BufInfo *b = C3D_GetBufInfo();
  BufInfo_Init(b);
  BufInfo_Add(b, vertices, sizeof(Vertex), 5, 0x43210);top_buffer=*b;
  C3D_CullFace(GPU_CULL_NONE);
  C3D_DepthMap(true, -1, 0);
  C3D_TexEnvBufUpdate(C3D_Both, 0);
}
static void end() {
  flush_draw();
  queue_stereo_depth();
  sharp_present();
  const u64 frame_cpu_ticks=svcGetSystemTick()-cpu_frame_start;
  static u64 window_cpu_ticks,window_wait_ticks,window_peak_ticks;
  static unsigned window_samples;
  if(!measure_frame) {
    window_cpu_ticks+=frame_cpu_ticks;window_wait_ticks+=cpu_frame_start-gpu_wait_start;
    window_peak_ticks=std::max(window_peak_ticks,frame_cpu_ticks);window_samples++;
  }else if(window_samples) {
    const double ms=1000.0/SYSCLOCK_ARM11;
    sysLogPrintf(2,"Render window: cpuAvg=%.2fms cpuPeak=%.2fms GPUwaitAvg=%.2fms samples=%u (excludes detailed probe frames)",
        window_cpu_ticks*ms/window_samples,window_peak_ticks*ms,window_wait_ticks*ms/window_samples,window_samples);
    window_cpu_ticks=window_wait_ticks=window_peak_ticks=0;window_samples=0;
  }

  __atomic_store_n(&gpu_phase,3,__ATOMIC_RELEASE);
  if(frame_count%120==0)
    sysLogPrintf(2,"PICA frame %u: draws=%u vertices=%u bright=%u inClip=%u viewport=%d,%d,%d,%d scissor=%d,%d,%d,%d depth=%d blend=%d",
                 frame_count,draw_calls,(unsigned)vertex_count,bright_vertices,clip_vertices,
                 last_viewport[0],last_viewport[1],last_viewport[2],last_viewport[3],
                 last_scissor[0],last_scissor[1],last_scissor[2],last_scissor[3],last_depth,last_blend);
  if(frame_count%120==0 && stereo_active)sysLogPrintf(2,"Stereo depth: convergence=%.1f reticle=%.2fpx scene=%u",double(world_convergence),double(reticle_shift*200.f),depth_frame.triangles);
  if(frame_count%120==0)sysLogPrintf(2,"Render cost: cpuWall=%.2fms wait=%.2fms previousGPU=%.2fms cmd=%.1f%% TEVconfig=%u reuse=%u",
    (double)frame_cpu_ticks*1000.0/SYSCLOCK_ARM11,gpu_wait_ms,
    (double)C3D_GetDrawingTime(),(double)C3D_GetCmdBufUsage()*100.0,tev_configs,tev_reuses);
  if(frame_count%120==0) {
    sysLogPrintf(2,"GPU indexed models: draws=%u vertexCopiesAvoided=%u indices=%u",indexed_draws,indexed_saved,(unsigned)world_index_count);
    sysLogPrintf(2,"CPU room arrays: draws=%u (indexed CPU draws disabled)",cpu_array_draws);
    sysLogPrintf(2,"GPU world backend: draws=%u",gpu_world_draws);
    sysLogPrintf(2,"GPU matrix palette: switches=%u pendingPreserved=%u rollovers=%u",world_palette_switches,world_palette_continued,world_palette_rollovers);
    sysLogPrintf(2,"Retained GPU meshes: hits=%u misses=%u reusedVertices=%u builtVertices=%u busy=%u bytes=%u",retained_meshes.hits,retained_meshes.misses,retained_meshes.reused,retained_meshes.built,retained_meshes.busy,(unsigned)retained_meshes.bytes());
    sysLogPrintf(2,"Input planning: provenDraws=%u scannedDraws=%u",proven_input_draws,scanned_input_draws);
    sysLogPrintf(2,"Camera LOD cache: draws=%u hits=%u",mesh_camera_lod_draws,mesh_camera_lod_hits);
    sysLogPrintf(2,"Static GPU attributes: hits=%u misses=%u previousFrame=%u bytes=%u",static_attributes.hits,static_attributes.misses,static_attributes.cross_frame,(unsigned)sizeof static_attributes);
    sysLogPrintf(2,"Vertex conversion: reused=%u built=%u",converted_reused,converted_built);
    sysLogPrintf(2,"GPU mesh cache: hits=%u misses=%u reusedVertices=%u convertedVertices=%u busy=%u bytes=%u",mesh_cache.hits,mesh_cache.misses,mesh_cache.reused,mesh_cache.converted,mesh_cache.blocked,(unsigned)mesh_cache.bytes());
    sysLogPrintf(2,"Texture LOD: full=%u mid=%u far=%u distant=%u promoted=%u (levels 1/2/3, minimum 8 texels)",texture_lod_draws[0],texture_lod_draws[1],texture_lod_draws[2],texture_lod_draws[3],texture_lod_promotions);
    uint32_t cached,file;native_cart_stats(&cached,&file);
    sysLogPrintf(2,"Cartridge DMA: cached=%lu file=%lu",(unsigned long)cached,(unsigned long)file);
    const double ms=1000.0/SYSCLOCK_ARM11;
    sysLogPrintf(2,"Clip census: near=%u far=%u behind=%u",near_vertices,far_vertices,behind_vertices);
    sysLogPrintf(2,"Material cache: hits=%u builds=%u; GPU submissions=%u merged=%u",material_hits,material_builds,gpu_draws,merged_draws);
    sysLogPrintf(2,"Retained frame stream: parts=%u",retained_streamed_parts);
    sysLogPrintf(2,"Postprocess: filter=%d vignette=%d bloom=%d grain=%d enabled=%d draws=%u",sharp_strength,post_vignette,post_bloom,post_grain,sharp_enabled,sharp_enabled?((sharp_strength?3u:1u)+(post_bloom?4u:0u)+(post_vignette && native_vignette_active(post_vignette_mode,native_setting_render_state())?1u:0u)+(post_grain?2u:0u))*(stereo_active?2u:1u):0u);
    sysLogPrintf(2,"AA: %s (3DS 2x downsample; N64 coverage filter not emulated)",top_sample_scale==2?"enabled":"disabled");
    sysLogPrintf(2,"Render breakdown: backend=%.2fms planning=%.2fms upload=%.2fms (sampled 1/120 frames)",
                 draw_ticks*ms,planning_ticks*ms,upload_ticks*ms);
  }
  mesh_cache.flush();
  if(!retained_stream_enabled())retained_meshes.flush();
  GSPGPU_FlushDataCache(vertices, vertex_count * sizeof(Vertex));
  if(world_index_count)GSPGPU_FlushDataCache(world_indices,world_index_count*sizeof(uint16_t));
  if(watch_seen)GSPGPU_FlushDataCache(watch_vertices,vertex_count*sizeof(Vertex));
  if(!watch_seen && watch_attached)watch_end_output();
  uint64_t native_submit_start=native_work_profile_begin();
  C3D_FrameEnd(0);
  native_work_profile_end(NATIVE_WORK_SUBMIT,native_submit_start);
  if(measure_frame && native_platform_log_batch_end)native_platform_log_batch_end();
  __atomic_store_n(&gpu_phase,0,__ATOMIC_RELEASE);
  in_frame = false;
  __atomic_add_fetch(&frame_count,1u,__ATOMIC_RELEASE);
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
  flush_draw();
  if(depth && !depth_copied)depth_frame.usable=false;
  if(stereo_active)C3D_RenderTargetClear(right_target,(C3D_ClearBits)((color?C3D_CLEAR_COLOR:0)|(depth?C3D_CLEAR_DEPTH:0)),0x000000ff,0);
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
  flush_draw();
  if (id >= textures.size() || !textures[id])
    return;
  if (textures[id]->ready){release_texture_lod_budget(textures[id]);dispose(textures[id]->gpu);}
  delete textures[id];
  textures[id] = nullptr;
}
static void filter(FilteringMode m) {
  flush_draw();
  if (m != FILTER_NONE && m != FILTER_LINEAR)
    unsupported("advanced filtering");
  filtering = m;
}
static FilteringMode get_filter() { return filtering; }
static void mipmap(MipmapFilteringMode m) { flush_draw();mip_filter = m; }
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
extern "C" unsigned native_graphics_frames() { return __atomic_load_n(&frame_count,__ATOMIC_ACQUIRE); }
extern "C" unsigned native_graphics_triangles() { return triangle_count; }
extern "C" unsigned native_graphics_uploads() { return upload_count; }
extern "C" void native_graphics_shutdown() {
  if (in_frame)
    end();
  // Detaching outside a frame waits for the GPU transfer/draw queue, before
  // texture memory used by the last room frame can be freed.
  if(sharp_enabled){for(auto *p:sharp_output)if(p)C3D_RenderTargetDetachOutput(p);}
  else {C3D_RenderTargetDetachOutput(target);C3D_RenderTargetDetachOutput(right_target);}
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
  linearFree(depth_copy);depth_copy=nullptr;
  linearFree(watch_vertices);watch_vertices=nullptr;
  watch_end_output();
  C3D_RenderTargetDelete(watch_target);watch_target=nullptr;
  mesh_cache.clear();
  retained_meshes.clear();
  if(world_indices)linearFree(world_indices);
  world_indices=nullptr;
  linearFree(vertices);
  vertices = nullptr;
  shaderProgramFree(&world_program);DVLB_Free(world_shader);
  shaderProgramFree(&program);
  DVLB_Free(shader);
  sharp_release_targets();
  if(sharp_vertices){linearFree(sharp_vertices);sharp_vertices=nullptr;}
  C3D_Fini();
}

/* Called only after engine quit and renderer idle. Keep engine-owned caches
 * allocated until process exit, but remove Citro3D GSP/APT callbacks first. */
extern "C" void native_graphics_stop_presenting(void) {
  watch_end_output();
  if(sharp_enabled){for(auto *p:sharp_output)if(p)C3D_RenderTargetDetachOutput(p);}
  else {C3D_RenderTargetDetachOutput(target);C3D_RenderTargetDetachOutput(right_target);}
  C3D_Fini();
}
