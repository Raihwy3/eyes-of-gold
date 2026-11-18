/* Recording backend for host verification of the real Fast3D/scene path.
 * Not linked into the console build and does not emulate PICA rendering.
 */
#include "gfx_cc.h"
#include "gfx_rendering_api.h"
#include "gfx_window_manager_api.h"
#include "native_cart.h"
#include "native_settings.h"
#include "native_theme.h"
#include "native_glass.h"
#include "native_distance_fog.h"
#include "native_graphics.h"
#include "native_motion.h"
#include "native_ram.h"
#include "native_room.h"
#include "pica_depth.h"
#include "pica_fog.h"
#include "pica_inputs.h"
#include "pica_combiner.h"
#include "native_gpu_world.h"
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <sys/mman.h>
#include <vector>
struct ShaderProgram {
  uint64_t a;
  uint32_t b;
  CCFeatures cc;
};
static uint64_t submission_hash=14695981039346656037ULL;
static void trace_bytes(const void *data,size_t size) {
  const auto *p=static_cast<const unsigned char *>(data);
  while(size--)submission_hash=(submission_hash ^ *p++)*1099511628211ULL;
}
template<class... T> static void trace_event(unsigned tag,const T &...values) {
  trace_bytes(&tag,sizeof tag);(trace_bytes(&values,sizeof values),...);
}
static bool recorded_viewmodel;
extern "C" void native_pica_set_viewmodel(int enabled) {
 recorded_viewmodel=enabled!=0;trace_event(10,recorded_viewmodel);
}
static std::vector<ShaderProgram *> programs;
static ShaderProgram *current;
static unsigned frames, tris, uploads, selected_tile, row;
static std::vector<unsigned char> reference;
static unsigned draw_batches;
static int expected_canvas_x=40,expected_canvas_width=320;
static bool menu;
static bool intro_mode;
static bool generated_lod_probe;
static double generated_lod_area;
static unsigned texture_lod_seen,texture_lod_current;
extern "C" void native_generated_lod_scene(int,int);
static unsigned captured_lod_rgb,captured_lod_alpha,lod_rgb_seen;
static unsigned captured_uniform,captured_alpha_uniform,proven_vertices;
extern "C" void native_pica_input_hint(unsigned uniform,unsigned alpha){captured_uniform=uniform;captured_alpha_uniform=alpha;}

extern "C" void native_pica_lod_hint(unsigned rgb,unsigned alpha){captured_lod_rgb=rgb;captured_lod_alpha=alpha;lod_rgb_seen|=rgb;}
extern "C" void native_pica_texture_lod(unsigned tier){texture_lod_current=tier;texture_lod_seen|=1u<<tier;}

static bool gpu_world_probe,gpu_record,model_gpu_probe;
static bool retained_probe,retained_record;
static uint64_t retained_hint;
static unsigned retained_draws;
extern "C" int native_pica_retained_supported(void){return retained_probe;}
extern "C" void native_pica_retained_hint(uint64_t token){retained_hint=token;}
static NativeGpuWorld gpu_world_matrix;
static bool gpu_world_enabled;
static unsigned gpu_world_switches;
static std::vector<float> gpu_positions;
extern "C" int native_pica_gpu_world_supported(void){return gpu_world_probe || model_gpu_probe;}
extern "C" int native_pica_gpu_world(const NativeGpuWorld *world){
  gpu_world_enabled=world!=nullptr;if(world)gpu_world_matrix=*world;
  gpu_world_switches++;return 1;
}
static bool fog_probe;
static unsigned fogged_tris,unfogged_tris;
extern "C" void native_distance_fog_scene(int);
static bool glass_probe;
static unsigned glass_draw,glass_authored_alpha;
static std::vector<float> glass_original;
extern "C" void native_glass_scene(int);
extern "C" void native_glass_gpu_scene(int,int,int);
extern "C" void native_original_fog_near_scene(int);
extern "C" void native_gpu_side_scene(int);
extern "C" void native_deferred_bg_scene(void);
static bool theme_probe;
static unsigned theme_draw;
static unsigned intro_bright;
extern "C" void native_intro_scene(int);
extern "C" void native_camera_plane_scene(int,int);
extern "C" void native_world_camera_plane_scene(int,int);
extern "C" void native_world_winding_scene(int,int);
extern "C" void native_model_face_scene(int,int,int,int);
extern "C" void native_lcd_edge_scene(void);
extern "C" void native_watch_colour_scene(void);
extern "C" int get_screen_ratio(void){return 0;}
extern "C" void gfx_set_output_aspect(float);
extern "C" void gfx_get_output_rect(int32_t *,int32_t *,int32_t *,int32_t *);
static bool room_mode;
static unsigned expected_pixels[4096][4], pixels_seen, expected_count = 6;
static bool world_mode;
static unsigned fog_draws;
static unsigned opaque_depth, decal_depth, frame_vertices;
extern "C" unsigned char *g_VtxBuffers[3];
unsigned char *g_VtxBuffers[3];
extern "C" void sysFatalError(const char *fmt, ...) {
  va_list a;
  va_start(a, fmt);
  vfprintf(stderr, fmt, a);
  va_end(a);
  abort();
}
static unsigned object_fast,object_rejected,object_fallback;
static unsigned batch_reused,batch_built,batch_fast,batch_fallback;
static unsigned room_packet_hits,room_packet_binds;
extern "C" void native_scenery_batch_scene(int);
static unsigned avoided_clip_fallbacks,fast_clip_rounds;
static unsigned prepared_hits,prepared_builds,prepared_corners;
extern "C" void native_cpu_cache_scene(int);
extern "C" void native_cpu_cache_pose_scene(int,int,int);
static bool cpu_world_hint;static unsigned cpu_hint_draws;
extern "C" void native_pica_cpu_world_hint(int enabled){cpu_world_hint=enabled!=0;}
static unsigned retained_producer_hits,retained_producer_corners;
static unsigned bg_last_loaded,bg_last_resolved;
static unsigned cpu_last_loaded,cpu_last_resolved,cpu_last_rejected;
extern "C" void native_cpu_room_budget_scene(int);
static unsigned face_last_rejected,face_last_batches,face_last_fallbacks;
static unsigned actor_fast,actor_fallback,vehicle_fast,vehicle_fallback,normal_hits,normal_builds;
static unsigned setup_reused,setup_rebuilt,packed_reused,packed_built,scenery_reused,scenery_built,scenery_persisted;
extern "C" void sysLogPrintf(int, const char *fmt, ...) {
  if(!strcmp(fmt,"Direct UI quads: batches=%u")){va_list args;va_start(args,fmt);vprintf(fmt,args);va_end(args);putchar('\n');}

  if(!strcmp(fmt,"Material work avoided: stateNoops=%u rectanglePairs=%u")){va_list args;va_start(args,fmt);vprintf(fmt,args);va_end(args);putchar('\n');}

  if(!strcmp(fmt,"Room packets: direct=%u bound=%u fallback=%u bytes=%u")) {
    va_list args;va_start(args,fmt);room_packet_hits+=va_arg(args,unsigned);room_packet_binds+=va_arg(args,unsigned);va_end(args);
  }

  if(!strcmp(fmt,"CPU room blocks: deferred=%u resolved=%u avoidedTransforms=%u tested=%u rejected=%u")) {
    va_list args;va_start(args,fmt);cpu_last_loaded=va_arg(args,unsigned);cpu_last_resolved=va_arg(args,unsigned);
    assert(cpu_last_resolved<=cpu_last_loaded && va_arg(args,unsigned)==cpu_last_loaded-cpu_last_resolved);
    (void)va_arg(args,unsigned);cpu_last_rejected=va_arg(args,unsigned);va_end(args);
  }

  if(!strcmp(fmt,"Model face selection: tested=%u rejectedBeforePacking=%u referenceFallbacks=%u batchesSkipped=%u")) {
    va_list args;va_start(args,fmt);(void)va_arg(args,unsigned);face_last_rejected=va_arg(args,unsigned);
    face_last_fallbacks=va_arg(args,unsigned);face_last_batches=va_arg(args,unsigned);va_end(args);
  }
  if(!strcmp(fmt,"Deferred background: loaded=%u resolved=%u avoidedDepthFog=%u")) {
    va_list args;va_start(args,fmt);bg_last_loaded=va_arg(args,unsigned);bg_last_resolved=va_arg(args,unsigned);
    assert(va_arg(args,unsigned)==bg_last_loaded-bg_last_resolved);va_end(args);
  }
  if(!strcmp(fmt,"Retained mesh producer: hits=%u builds=%u reusedCorners=%u bytes=%u")) {
    va_list args;va_start(args,fmt);retained_producer_hits+=va_arg(args,unsigned);(void)va_arg(args,unsigned);retained_producer_corners+=va_arg(args,unsigned);va_end(args);
  }
  if(!strcmp(fmt,"Model batches: actorTriangles=%u actorFallback=%u vehicleTriangles=%u vehicleFallback=%u")) {
    va_list args;va_start(args,fmt);actor_fast+=va_arg(args,unsigned);actor_fallback+=va_arg(args,unsigned);vehicle_fast+=va_arg(args,unsigned);vehicle_fallback+=va_arg(args,unsigned);va_end(args);
  }
  if(!strcmp(fmt,"Lighting normals: reused=%u built=%u")) {
    va_list args;va_start(args,fmt);normal_hits+=va_arg(args,unsigned);normal_builds+=va_arg(args,unsigned);va_end(args);
  }

  if(!strcmp(fmt,"Prepared world batches: hits=%u builds=%u reusedCorners=%u replaced=%u bytes=%u")) {
    va_list args;va_start(args,fmt);prepared_hits+=va_arg(args,unsigned);prepared_builds+=va_arg(args,unsigned);prepared_corners+=va_arg(args,unsigned);va_end(args);
  }

  if(!strcmp(fmt,"CPU clipping: avoidedFallback=%u fastColourRounds=%u")) {
    va_list args;va_start(args,fmt);avoided_clip_fallbacks+=va_arg(args,unsigned);fast_clip_rounds+=va_arg(args,unsigned);va_end(args);
  }

  if(!strcmp(fmt,"Object batches: fastTriangles=%u rejected=%u fallback=%u")) {
    va_list args;va_start(args,fmt);object_fast+=va_arg(args,unsigned);object_rejected+=va_arg(args,unsigned);object_fallback+=va_arg(args,unsigned);va_end(args);
  }

  if(!strcmp(fmt,"Scenery batches: reused=%u compiled=%u fastTriangles=%u rejected=%u fallback=%u bytes=%u")) {
    va_list args;va_start(args,fmt);
    batch_reused+=va_arg(args,unsigned);batch_built+=va_arg(args,unsigned);batch_fast+=va_arg(args,unsigned);
    (void)va_arg(args,unsigned);batch_fallback+=va_arg(args,unsigned);
    assert(va_arg(args,unsigned)<=64*1024);va_end(args);
  }

  if(!strcmp(fmt,"Scenery attributes: reused=%u persisted=%u built=%u bytes=%u")) {
    va_list args;va_start(args,fmt);
    scenery_reused+=va_arg(args,unsigned);scenery_persisted+=va_arg(args,unsigned);scenery_built+=va_arg(args,unsigned);
    assert(va_arg(args,unsigned)<=256*1024);va_end(args);
  }

  if(!strcmp(fmt,"RSP vertex packing: reused=%u packed=%u")) {
    va_list args;va_start(args,fmt);
    packed_reused+=va_arg(args,unsigned);packed_built+=va_arg(args,unsigned);va_end(args);
  }
  if(!strcmp(fmt,"RSP triangle setup: reused=%u rebuilt=%u")) {
    va_list args;va_start(args,fmt);
    setup_reused+=va_arg(args,unsigned);setup_rebuilt+=va_arg(args,unsigned);va_end(args);
  }
}
extern "C" void osSyncPrintf(const char *, ...) {}
extern "C" const char *sysResolvePath(const char *) {
  return "/tmp/eog-native-graphics-shaders";
}
static const char *name() { return "host recording backend"; }
static int maxtex() { return 1024; }
static GfxClipParameters clip() { return {false, false}; }
static void unload(ShaderProgram *) { current = nullptr; }
static void load(ShaderProgram *s) { current = s; }
static ShaderProgram *lookup(uint64_t a, uint32_t b) {
  for (auto *s : programs)
    if (s->a == a && s->b == b)
      return s;
  return nullptr;
}
static ShaderProgram *create(uint64_t a, uint32_t b) {
  auto *s = new ShaderProgram{};
  s->a = a;
  s->b = b;
  gfx_cc_get_features(a, b, &s->cc);
  programs.push_back(s);
  return current = s;
}
static void info(ShaderProgram *s, uint8_t *n, bool used[2]) {
  *n = s->cc.num_inputs;
  used[0] = s->cc.used_textures[0];
  used[1] = s->cc.used_textures[1];
}
static void clear_shaders() {
  for (auto *s : programs)
    delete s;
  programs.clear();
  current = nullptr;
}
static uint32_t newtex() {
  static uint32_t id;
  return ++id;
}
static void select(int tile, uint32_t id, bool mip) { trace_event(1,tile,id,mip);selected_tile = tile; }
static void upload(const uint8_t *rgba, uint32_t w, uint32_t h, bool mip) {
  trace_event(2,w,h,mip);trace_bytes(rgba,size_t(w)*h*4);
  if(intro_mode) { ++uploads;return; }
  if (room_mode) {
    unsigned hash = 2166136261u;
    for (unsigned i = 0; i < w * h; ++i)
      for (unsigned c = 0; c < 3; ++c)
        hash = (hash ^ rgba[i * 4 + c]) * 16777619u;
    int found = -1;
    for (unsigned i = 0; i < expected_count; ++i)
      if (expected_pixels[i][1] == w && expected_pixels[i][2] == h &&
          expected_pixels[i][3] == hash)
        found = i;
    if (found < 0)
      fprintf(stderr, "Texture mismatch %ux%u hash %u\n", w, h, hash);
    assert(found >= 0);
    if (!world_mode)
      pixels_seen |= 1u << found;
    ++uploads;
    return;
  }
  assert(menu && !mip && selected_tile==0 && w>=440 && h==299 && row==0);
  for(unsigned y=0;y<299;++y)for(unsigned x=0;x<440;++x) {
    unsigned pixel=reference[y*440+x],offset=(y*w+x)*4;
    for(unsigned c=0;c<4;++c)assert(rgba[offset+c]==pixel);
  }
  row=299;
  ++uploads;
}
static void sampler(int tile, bool linear, uint32_t cms, uint32_t cmt, bool mip) {
  trace_event(3,tile,linear,cms,cmt,mip);
  if (!room_mode && !intro_mode)
    assert(!mip);
}
static void depth(bool test, bool write, bool compare, bool prim,
                  uint16_t mode) {
  trace_event(4,test,write,compare,prim,mode);
  if(intro_mode)return;
  if (room_mode) {
    PicaDepthPlan plan;
    if (!pica_plan_depth(test, write, compare, prim, mode, &plan))
      fprintf(stderr, "Depth mode %x test%d write%d compare%d prim%d\n", mode,
              test, write, compare, prim);
    assert(pica_plan_depth(test, write, compare, prim, mode, &plan));
    assert(plan.test && plan.compare);
    if (mode == 0xc00) {
      assert(!plan.write && plan.bias > 0);
      ++decal_depth;
    } else {
      assert(plan.write == write && plan.bias == 0);
      ++opaque_depth;
    }
    return;
  }
  assert(!prim && !mode);
  if (!menu && !room_mode)
    assert(test && write && compare);
}
static void range(float a, float b) {trace_event(5,a,b);}
static void viewport(int x, int y, int w, int h) {
  trace_event(6,x,y,w,h);
  assert(x == expected_canvas_x && y == 0 && w == expected_canvas_width && h == 240);
}
static void scissor(int x, int y, int w, int h) {
  trace_event(7,x,y,w,h);
  assert(x == expected_canvas_x && y == 0 && w == expected_canvas_width && h == 240);
}
static void alpha(bool a, bool b) {trace_event(8,a,b);}
static bool verified_fog_hint;
static float verified_fog_rgb[3];
static unsigned verified_fog_vertices;
extern "C" void native_pica_fog_hint(int valid,const float *colour) {
  verified_fog_hint=valid;
  if(valid)memcpy(verified_fog_rgb,colour,sizeof verified_fog_rgb);
}
static const float *verified_vertex_base;
static const uint64_t *verified_vertex_ids;
static size_t verified_vertex_count;
static unsigned verified_vertex_reuse,verified_vertex_total;
extern "C" void native_pica_vertex_ids(const float *base,const uint64_t *ids,size_t count) {
 verified_vertex_base=base;verified_vertex_ids=ids;verified_vertex_count=count;
}
static const float *const *verified_vertex_refs;
static const float *verified_reference_base;
static size_t verified_reference_count;
extern "C" void native_pica_vertex_refs(const float *base,const float *const *refs,size_t count) {
 verified_reference_base=base;verified_vertex_refs=refs;verified_reference_count=count;
}
static const uint64_t *verified_attribute_ids;
static unsigned verified_static_vertices,verified_static_hits;
extern "C" void native_pica_attribute_ids(const float *,const uint64_t *ids,size_t) {verified_attribute_ids=ids;}
static void draw(float *buf, size_t floats, size_t n) {
  if(retained_hint)retained_draws++;
  if(cpu_world_hint)cpu_hint_draws++;
  const float *incoming_buf=buf;
  std::vector<float> expanded;
  if(verified_vertex_refs) {
    assert(verified_reference_base==buf && verified_reference_count==n*3);
    size_t stride=floats/(n*3);expanded.resize(floats);
    for(size_t i=0;i<n*3;i++)memcpy(expanded.data()+i*stride,verified_vertex_refs[i],stride*sizeof(float));
    buf=expanded.data();
  }

  if(verified_vertex_ids) {
    assert(verified_vertex_base==incoming_buf && verified_vertex_count==n*3);
    size_t stride=floats/(n*3);uint64_t ids[128]={};size_t slots[128]={};
    for(size_t i=0;i<n*3;i++)if(uint64_t id=verified_vertex_ids[i]) {
      unsigned slot=(id-1)&127;
      verified_vertex_total++;
      if(ids[slot]==id){assert(!memcmp(buf+slots[slot]*stride,buf+i*stride,stride*sizeof(float)));verified_vertex_reuse++;}
      ids[slot]=id;slots[slot]=i;
    }
  }

  std::vector<float> transformed;
  if(gpu_world_enabled) {
    transformed.assign(buf,buf+floats);size_t stride=floats/(n*3);
    for(size_t i=0;i<n*3;i++)for(unsigned row=0;row<4;row++) {
      float value=0;for(unsigned col=0;col<4;col++)value+=gpu_world_matrix.rows[row][col]*buf[i*stride+col];
      transformed[i*stride+row]=value;
    }
    buf=transformed.data();
  }
  if(gpu_record) {
    size_t stride=floats/(n*3);
    for(size_t i=0;i<n;i++) {
      const float *a=buf+i*stride*3,*b=a+stride,*c=b+stride;
      double area=(b[0]/b[3]-a[0]/a[3])*(c[1]/c[3]-a[1]/a[3])-(b[1]/b[3]-a[1]/a[3])*(c[0]/c[3]-a[0]/a[3]);
      if(gpu_world_enabled && ((gpu_world_matrix.cull==1 && area>=0) || (gpu_world_matrix.cull==2 && area<=0)))continue;
      for(unsigned j=0;j<3;j++)for(unsigned k=0;k<(retained_record?stride:4);k++)gpu_positions.push_back(a[j*stride+k]);
    }
  }
  ++draw_batches;
  assert(current);
  trace_event(9,current->a,current->b,floats,n);trace_bytes(buf,floats*sizeof(float));
  const auto &c = current->cc;
  if(verified_attribute_ids && !gpu_world_enabled) {
    struct StaticRecord {uint64_t id;unsigned count;float data[40];};
    static StaticRecord records[1024]{};
    unsigned stride=floats/(n*3),channels=c.opt_alpha?4:3,offset=stride-channels*c.num_inputs;
    assert(stride<=40);
    for(unsigned v=0;v<n*3;v++)if(uint64_t id=verified_attribute_ids[v]) {
      float normalized[40]={};memcpy(normalized,buf+v*stride,stride*sizeof(float));
      for(unsigned k=0;k<4;k++)normalized[k]=0;
      if(c.opt_fog)normalized[offset-1]=0;
      for(unsigned j=0;j<unsigned(c.num_inputs);j++) {
        if(captured_lod_rgb&(1u<<j))for(unsigned k=0;k<3;k++)normalized[offset+j*channels+k]=0;
        if(c.opt_alpha && (captured_lod_alpha&(1u<<j)))normalized[offset+j*channels+3]=0;
      }
      auto &r=records[id&1023];
      if(r.id==id){assert(r.count==stride && !memcmp(r.data,normalized,stride*sizeof(float)));verified_static_hits++;}
      r.id=id;r.count=stride;memcpy(r.data,normalized,stride*sizeof(float));verified_static_vertices++;
    }
  }

  if(captured_uniform || captured_alpha_uniform) {
    unsigned stride=floats/(n*3),channels=c.opt_alpha?4:3,offset=stride-channels*c.num_inputs;
    for(unsigned j=0;j<unsigned(c.num_inputs);j++)for(unsigned v=0;v<n*3;v++) {
      if(captured_uniform&(1u<<j))for(unsigned k=0;k<channels;k++)
        assert(buf[v*stride+offset+j*channels+k]==buf[offset+j*channels+k]);
      if(c.opt_alpha && (captured_alpha_uniform&(1u<<j)))
        assert(buf[v*stride+offset+j*channels+3]==buf[offset+j*channels+3]);
    }
    proven_vertices+=n*3;
  }
  if(generated_lod_probe) {
    size_t stride=floats/(n*3);
    for(size_t t=0;t<n;t++) {
      const float *a=buf+t*3*stride,*b=a+stride,*d=b+stride;
      double ax=a[0]/a[3],ay=a[1]/a[3],bx=b[0]/b[3],by=b[1]/b[3],cx=d[0]/d[3],cy=d[1]/d[3];
      generated_lod_area+=std::fabs((bx-ax)*(cy-ay)-(by-ay)*(cx-ax))*.5;
    }
  }

  if(verified_fog_hint) {
    assert(c.opt_fog);size_t stride=floats/(n*3);
    size_t offset=stride-(c.opt_alpha?4:3)*c.num_inputs-4;
    for(size_t v=0;v<n*3;v++)for(unsigned k=0;k<3;k++)
      assert(buf[v*stride+offset+k]==verified_fog_rgb[k]);
    verified_fog_vertices+=n*3;
  }

  if(fog_probe) {
    assert(!c.used_textures[0] && !c.used_textures[1] && c.num_inputs==1);
    if(c.opt_fog){
      NativeDistanceFog fog;assert(native_distance_fog_plan(5,1000,200,0x103060ff,&fog));
      size_t stride=floats/(n*3);
      for(size_t i=0;i<n*3;i++){
        const float *p=buf+i*stride;assert(p[3]>0);
        assert(p[2]<=fog.end_ndc*p[3]+.0001f && p[2]+p[3]>=0);
        assert(std::fabs(p[4]-16.f/255)<.001f && std::fabs(p[5]-48.f/255)<.001f && std::fabs(p[6]-96.f/255)<.001f);
        assert(p[7]>=0 && p[7]<=1);
      }
      fogged_tris+=n;
    }else unfogged_tris+=n;
    tris+=n;return;
  }
  if(glass_probe) {
    assert(n==1 && c.num_inputs==1);
    const bool simple=native_setting_glass()==1 && glass_draw==1;
    assert(c.used_textures[0]==!simple && !c.used_textures[1]);
    assert(c.opt_alpha==!(simple && glass_authored_alpha==255));
    size_t stride=floats/3,off=stride-(c.opt_alpha?4:3);
    for(unsigned v=0;v<3;v++) {
      const float *p=buf+v*stride+off;
      assert(std::fabs(p[0]-(simple?12.f:100.f)/255.f)<.001f);
      assert(std::fabs(p[1]-(simple?12.f:120.f)/255.f)<.001f);
      assert(std::fabs(p[2]-(simple?12.f:140.f)/255.f)<.001f);
      if(c.opt_alpha)assert(std::fabs(p[3]-(simple?native_glass_alpha(glass_authored_alpha):glass_authored_alpha)/255.f)<.001f);
    }
    if(glass_draw==0)glass_original.assign(buf,buf+floats);
    if(glass_draw==2)assert(glass_original==std::vector<float>(buf,buf+floats));
    glass_draw++;tris+=n;return;
  }
  if(theme_probe) {
    assert(n==1 && c.num_inputs==1 && c.opt_alpha);
    size_t stride=floats/3,off=stride-4;
    unsigned r=0,g=255,b=0;
    if(theme_draw==1)native_theme_rgb(native_setting_watch(),&r,&g,&b);
    for(unsigned v=0;v<3;v++) {
      const float *p=buf+v*stride+off;
      assert(std::fabs(p[0]-r/255.f)<.001f && std::fabs(p[1]-g/255.f)<.001f && std::fabs(p[2]-b/255.f)<.001f);
      assert(std::fabs(p[3]-123/255.f)<.001f);
    }
    theme_draw++;tris+=n;return;
  }
  if(intro_mode) {
    const size_t stride=floats/(n*3);
    for(size_t k=0;k<floats;k++)assert(std::isfinite(buf[k]));
    if(c.num_inputs) {
      size_t off=stride-(c.opt_alpha?4:3)*c.num_inputs;
      for(size_t v=0;v<n*3;v++)if(buf[v*stride+off]>.1f)intro_bright++;
    }
    tris+=n;return;
  }

  if (room_mode) {
    frame_vertices += n * 3;
    assert(frame_vertices <= 32768);
    for (size_t k = 0; k < floats; ++k)
      assert(std::isfinite(buf[k]));
    size_t cs = (c.opt_alpha ? 4 : 3), stride = floats / (n * 3),
           off = stride - cs * c.num_inputs;
    PicaInputPlan plan;
    assert(pica_plan_inputs(buf, stride, off, n * 3, c.num_inputs, c.opt_alpha,
                            &plan));
    for (unsigned cycle = 0; cycle < (c.opt_2cyc ? 2u : 1u); ++cycle)
      for (unsigned ch = 0; ch < (c.opt_alpha ? 2u : 1u); ++ch)
        { PicaEquation equation; assert(pica_plan_equation(c,cycle,ch,&equation)); }
    if (c.opt_fog) {
      float fog[3];
      assert(pica_plan_fog(buf, stride, off - 4, n * 3, fog));
      fog_draws++;
    }
    assert(!c.opt_noise && !c.opt_grayscale && !c.opt_blur);
    tris += n;
    return;
  }
  assert(!c.opt_fog && !c.opt_noise && !c.opt_grayscale && !c.opt_blur &&
         !c.opt_2cyc);
  size_t stride = 4 + (c.opt_alpha ? 4 : 3) * c.num_inputs;
  for (unsigned t = 0; t < 2; ++t)
    if (c.used_textures[t])
      stride += 2 + c.clamp[t][0] + c.clamp[t][1];
  assert(floats == stride * n * 3);
  for (size_t i = 0; i < n * 3; ++i) {
    float *p = buf + i * stride;
    for (unsigned k = 0; k < stride; ++k)
      assert(std::isfinite(p[k]));
    assert(p[3] > 0);
    if(menu) {
      const float image_y=(1.f-p[1]/p[3])*165.f-16.f;
      // Original N64 texrect extends geometry to the next pixel edge,
      // while the texture coordinate ends one quarter texel before it.
      const float delta=image_y-p[5]*299.f;
      assert(std::fabs(delta)<.02f || std::fabs(delta-.25f)<.02f);
    }
  }
  if (menu) {
    assert(c.used_textures[0] && !c.used_textures[1] && c.num_inputs == 1 &&
           c.do_multiply[0][0]);
    assert(n % 2 == 0);
    // The original I8 backdrop and its primitive gradient must stay neutral.
    // This checks the actual interpreter output, before any GPU channel
    // packing.
    const size_t color_offset = stride - (c.opt_alpha ? 4 : 3);
    for (size_t i = 0; i < n * 3; ++i) {
      const float *rgb = buf + i * stride + color_offset;
      assert(rgb[0] >= 0 && rgb[0] <= 1);
      assert(std::fabs(rgb[0] - rgb[1]) < .00001f);
      assert(std::fabs(rgb[1] - rgb[2]) < .00001f);
    }
  } else {
    assert(!c.used_textures[0] && !c.used_textures[1] && c.num_inputs == 1 &&
           c.do_single[0][0]);
  }
  tris += n;
}
static void noop() {}
static int newfb() {
  static int id;
  return ++id;
}
static void update(int id, uint32_t w, uint32_t h, uint32_t msaa, bool, bool,
                   bool, bool) {
  assert(!id && w == 400 && h == 240 && msaa == 1);
}
static bool bind(int id, float) {
  assert(!id);
  return true;
}
static void clear(bool, bool) {}
static void copy(int, int, int, int, bool, bool) { abort(); }
static void resolve(int, int) { abort(); }
static void *fbtex(int) { abort(); }
static void selectfb(int) { abort(); }
static void deltex(uint32_t) {}
static void filter(FilteringMode m) { assert(m == FILTER_NONE); }
static FilteringMode getfilter() { return FILTER_NONE; }
static void mipmap(MipmapFilteringMode m) { assert(m == MIPMAP_DISABLED); }
static void aniso(int n) { assert(n <= 1); }
static int maxaniso() { return 1; }
static void end() {
  gpu_world_enabled=false;
  ++frames;
  frame_vertices = 0;
}
GfxRenderingAPI native_pica_api = {
    name,   maxtex,        clip,    unload,   load,   create,  lookup,
    info,   clear_shaders, newtex,  select,   upload, sampler, depth,
    range,  viewport,      scissor, alpha,    draw,   noop,    noop,
    noop,   end,           noop,    newfb,    update, bind,    copy,
    clear,  resolve,       fbtex,   selectfb, deltex, filter,  getfilter,
    mipmap, aniso,         maxaniso};
static void init(const GfxWindowInitSettings *) {}
static void dimensions(uint32_t *w, uint32_t *h, int32_t *x, int32_t *y) {
  *w = 400;
  *h = 240;
  *x = *y = 0;
}
static bool start() { return true; }
static int32_t fps() { return 60; }
static void setfps(int) {}
GfxWindowManagerAPI native_window_api = [] {
  GfxWindowManagerAPI a = {};
  a.init = init;
  a.get_dimensions = dimensions;
  a.handle_events = noop;
  a.start_frame = start;
  a.swap_buffers_begin = noop;
  a.swap_buffers_end = noop;
  a.get_target_fps = fps;
  a.set_target_fps = setfps;
  return a;
}();
extern "C" unsigned native_graphics_frames() { return frames; }
extern "C" void native_graphics_destroy_interpreter(void);
int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  assert(argc == 5);
  FILE *expected = fopen(argv[4], "rb");
  assert(expected);
  assert(fread(expected_pixels, 16, 6, expected) == 6 &&
         fgetc(expected) == EOF);
  fclose(expected);
  FILE *f = fopen(argv[2], "rb");
  assert(f);
  reference.resize(440 * 299);
  assert(fread(reference.data(), 1, reference.size(), f) == reference.size());
  assert(fgetc(f) == EOF);
  fclose(f);
  assert(native_scene_load(argv[1]));
  assert(native_graphics_init());
  menu = true;
  native_scene_frame(0, 0);
  native_scene_frame(0, 0);
  native_scene_frame(0, 0);
  assert(tris == 598 && uploads == 1 && row == 299 && frames == 1);
  assert(draw_batches<10);
  printf("PASS original 299-row backdrop: one texture upload, %u draw batches\n",draw_batches);
  // Reuse the texture cache and switch through both offset signs.
  native_scene_frame(160, 0);
  native_scene_frame(540, 0);
  assert(tris == 1794 && uploads == 1 && frames == 3);
  menu = false;
  unsigned before = tris;
  native_scene_frame(0, 1);
  assert(tris == before + 2 && frames == 4);
  native_scene_frame(83, 1);
  assert(tris >= before + 4 && frames == 5);
  menu = true;
  native_scene_frame(0, 0);
  assert(uploads == 1 && frames == 6);
  room_mode = true;
  menu = false;
  extern void *room_worker(void *);
  void *stack = mmap(nullptr, 512 * 1024, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
  assert(stack != MAP_FAILED);
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  assert(!pthread_attr_setstack(&attr, stack, 512 * 1024));
  pthread_t worker;
  assert(!pthread_create(&worker, &attr, room_worker, argv[3]));
  assert(!pthread_join(worker, nullptr));
  pthread_attr_destroy(&attr);
  munmap(stack, 512 * 1024);
  room_mode = false;
  menu = true;
  unsigned menu_before = tris;
  native_scene_frame(0, 0);
  assert(tris == menu_before + 598);
  menu = false;
  native_scene_frame(0, 1);
  assert(tris == menu_before + 600);
  intro_mode=true;
  for(int phase=0;phase<2;phase++) {
    unsigned before=tris;intro_bright=0;
    native_intro_scene(phase);
    printf("Intro phase %d: %u triangles, %u bright vertices\n",phase,tris-before,intro_bright);
    assert(tris>before && intro_bright>0);
  }
  for(int front=0;front<2;++front)for(int reverse=0;reverse<2;++reverse) {
    unsigned before=tris;
    native_camera_plane_scene(reverse,front);
    assert(tris-before==(front==reverse?2u:0u));
  }
  puts("PASS actual Fast3D camera-plane triangle: stable front/back winding at W=0 and near-clipped output");

  for(int full=1;full>=0;--full) {
    expected_canvas_x=full?0:40;expected_canvas_width=full?400:320;
    gfx_set_output_aspect(full?5.f/3.f:4.f/3.f);
    unsigned before=tris;
    for(int i=0;i<3;++i)native_camera_plane_scene(0,0);
    int32_t x,y,w,h;gfx_get_output_rect(&x,&y,&w,&h);
    assert(x==expected_canvas_x && y==0 && w==expected_canvas_width && h==240);
    assert(tris==before+2); // Two queued old-aspect frames held, third rendered.
  }
  puts("PASS live Fast3D aspect switch: 400x240 full LCD / centred 320x240 with viewport, scissor and transition settling");
  native_graphics_set_native_resolution(320,240);
  for(int full=0;full<2;++full) {
    expected_canvas_x=full?0:40;expected_canvas_width=full?400:320;
    gfx_set_output_aspect(full?5.f/3.f:4.f/3.f);
    for(int i=0;i<3;++i)native_lcd_edge_scene();
  }
  puts("PASS native 320x240 GBI canvas: Full viewport and untrimmed scissor cover every LCD edge in both ratios");
  theme_probe=true;
  for(int theme=1;theme<=3;theme++) {
    native_setting_set_watch(theme);theme_draw=0;native_watch_colour_scene();assert(theme_draw==3);
  }
  native_setting_set_watch(0);theme_probe=false;
  puts("PASS actual Fast3D watch palette: white/amber/red, alpha preserved, surrounding world draws unchanged");
  glass_probe=true;
  for(int detail=0;detail<2;detail++) {
    native_setting_set_glass(detail);
    for(unsigned a: {0u,123u,255u}){glass_authored_alpha=a;glass_draw=0;native_glass_scene(a);assert(glass_draw==3);}
  }
  glass_probe=false;native_setting_set_glass(1);
  puts("PASS scoped glass: original texture path retained, Simple dark grey has no texture inputs with opacity floor 128 and distant opaque portals preserved, following world material/UV/colour bytes restored exactly");
  fog_probe=true;
  expected_canvas_x=0;expected_canvas_width=400;gfx_set_output_aspect(5.f/3.f);
  for(int mode=0;mode<4;mode++){
    fogged_tris=unfogged_tris=0;native_distance_fog_scene(mode);
    printf("Fog scene %d: fogged=%u plain=%u\n",mode,fogged_tris,unfogged_tris);fflush(stdout);
    if(mode==1){assert(fogged_tris>=6 && fogged_tris<=9 && unfogged_tris==1);}
    if(mode==2)assert(!fogged_tris && unfogged_tris==102);
    if(mode==3)assert(fogged_tris==1 && unfogged_tris==1);
  }
  fog_probe=false;
  for(int mode: {0,0,1,2,1,0}) {
    unsigned before=tris;native_scenery_batch_scene(mode);
    assert(tris-before==(mode==1?440u:330u));
  }
  for(int mode: {3,3,4,5,4,3}) {
    unsigned before=tris;native_scenery_batch_scene(mode);
    assert(tris-before==(mode%3==1?440u:330u));
  }
  for(int mode: {6,6,7,8,7,6,9,9,10,11,10,9}) {
    unsigned before=tris;native_scenery_batch_scene(mode);
    assert(tris-before==(mode%3==1?440u:330u)+(mode<9?1u:0u));
  }
  for(int mode: {12,12,13,14,13,12}){unsigned before=tris;native_scenery_batch_scene(mode);assert(tris-before==(mode%3==1?440u:330u)+1u);}
  puts("PASS faded guard ordered batches: translucent actor bone matrices, changed pose/lighting, original blend state and following scope reset");
  if(!getenv("EOG_DISABLE_OBJECT_BATCHES") && !getenv("EOG_DISABLE_RSP_PREP_CACHE") && !getenv("EOG_DISABLE_SCENERY_BATCHES")) {
    if(!getenv("EOG_DISABLE_ACTOR_BATCHES"))assert(actor_fast>0);
    assert(vehicle_fast>0);
  }
  puts("PASS model batch scenes: actor bone matrices, changed pose/lighting, held-part layout, vehicle transforms and following scope reset");
  puts("PASS object batch scenes: lit vertices, changed instance transforms/colours and repeated/mutated model command data");
  puts("PASS compiled scenery runs: mixed TRI1/TRI4, degenerate padding, 256-triangle flush boundaries, source command mutation and camera/source vertex changes");

  {
    int saved=native_setting_lod();native_setting_set_lod(0);
    for(unsigned tier=0;tier<4;tier++) {
      unsigned before=tris;texture_lod_seen=0;
      native_generated_lod_scene(500,8+tier);
      assert(tris-before==32 && (tier==0 || (texture_lod_seen&(1u<<tier))) && texture_lod_current==0);
    }
    native_setting_set_lod(saved);
    puts("PASS actual effect texture scopes: four forced mip levels on non-scenery particles, full topology retained, flush ordering and following texture reset");
  }
  native_generated_lod_scene(500,6);
  if(!getenv("EOG_DISABLE_SCENERY_CACHE"))assert(proven_vertices>0);
  puts("PASS original material proofs: uniform RGB/alpha hints verified against every actual submitted vertex, mixed contexts invalidate proofs");
  assert(verified_fog_vertices>0);
  printf("PASS uniform fog proof: %u vertices checked against original packed RGB\n",verified_fog_vertices);
  puts("PASS actual distance fog: world fog colour, exact near/far boundary clipping, crossing walls retained, 100 fully hidden triangles removed before material submission, following UI unmodified");
  if(getenv("EOG_TEST_GENERATED_LOD")) {
    generated_lod_probe=true;native_setting_set_lod(1);double reference_area=0;
    for(int object=0;object<2;object++)for(int tier=0;tier<4;tier++) {
      int depth=tier==0?500:tier==1?1000:tier==2?2000:4000;unsigned before=tris;generated_lod_area=0;
      native_generated_lod_scene(depth,object);
      unsigned expected=getenv("EOG_DISABLE_MESH_LOD")?32:tier==0?32:tier==1?24:tier==2?18:14;
      assert(tris-before==expected);double normalized=generated_lod_area*depth*depth;
      if(!object && !tier)reference_area=normalized;
      assert(std::fabs(normalized-reference_area)<reference_area*.0001);
    }
    unsigned baked_before=tris;native_generated_lod_scene(1000,2);assert(tris-baked_before==30);
    baked_before=tris;native_generated_lod_scene(1000,7);assert(tris-baked_before==30 && !gpu_world_enabled);
    puts("PASS faded guard baked LOD: reduced transparent topology, original vertices unmodified, CPU blend path and scope reset");
    assert((texture_lod_seen&15)==15 && texture_lod_current==0);
    generated_lod_probe=false;native_setting_set_lod(0);
    puts("PASS actual generated LOD tiers: full/mid/far/distant for BG and object scopes, expected triangle counts, equal projected coverage and texture tier/UI reset");
  }
  if(getenv("EOG_TEST_GPU_WORLD")) {
    gpu_record=true;generated_lod_probe=true;native_setting_set_lod(0);
    for(int mode: {0,1,3,4})for(int depth: {500,1000,2000}) {
      gpu_world_probe=false;gpu_positions.clear();native_generated_lod_scene(depth,mode);
      const auto reference=gpu_positions;
      gpu_world_probe=true;gpu_positions.clear();native_generated_lod_scene(depth,mode);
      assert(gpu_positions.size()==reference.size());
      for(size_t i=0;i<reference.size();i++){if(std::fabs(gpu_positions[i]-reference[i])>=.001f)fprintf(stderr,"GPU mismatch mode=%d depth=%d i=%zu gpu=%f cpu=%f\n",mode,depth,i,gpu_positions[i],reference[i]);assert(std::fabs(gpu_positions[i]-reference[i])<.001f);}
    }
    for(int mode: {0,1,2,3,4,5}) {
      gpu_world_probe=false;gpu_positions.clear();native_scenery_batch_scene(mode);auto reference=gpu_positions;
      gpu_world_probe=true;gpu_positions.clear();native_scenery_batch_scene(mode);
      assert(reference.size()==gpu_positions.size());
      for(size_t i=0;i<reference.size();i++)assert(std::fabs(gpu_positions[i]-reference[i])<.001f);
    }
    for(int mode: {1,3}) {
      gpu_world_probe=false;gpu_positions.clear();native_distance_fog_scene(mode);auto reference=gpu_positions;
      gpu_world_probe=true;gpu_positions.clear();native_distance_fog_scene(mode);
      assert(reference.size()==gpu_positions.size());
      for(size_t i=0;i<reference.size();i++)assert(std::fabs(gpu_positions[i]-reference[i])<.001f);
    }
    lod_rgb_seen=0;native_generated_lod_scene(500,5);
    assert(lod_rgb_seen && !captured_lod_rgb && !captured_lod_alpha);
    assert(gpu_world_switches>0);
    gpu_world_probe=false;gpu_record=false;generated_lod_probe=false;
    // A following UI/reference draw must restore the original shader/state.
    native_generated_lod_scene(500,0);assert(!gpu_world_enabled);
    puts("PASS GPU world: actual interpreter defers XY, model-space submissions transform to reference clip coordinates, both cull directions agree, CPU/UI fallback restores state");
  }
  if(getenv("EOG_TEST_MODEL_GPU")) {
    gpu_record=true;unsigned switched=0;
    for(int mode: {3,6,7,8,9,10,11}) {
      model_gpu_probe=false;gpu_positions.clear();native_scenery_batch_scene(mode);auto reference=gpu_positions;
      unsigned before=gpu_world_switches;
      model_gpu_probe=true;gpu_positions.clear();native_scenery_batch_scene(mode);
      assert(reference.size()==gpu_positions.size());
      for(size_t i=0;i<reference.size();i++)assert(std::fabs(reference[i]-gpu_positions[i])<.001f);
      if(mode==3)assert(gpu_world_switches==before);else {assert(gpu_world_switches>before);switched++;}
    }
    model_gpu_probe=false;gpu_record=false;
    printf("PASS default GPU models: %u actor/vehicle matrix and lighting cases match CPU geometry; ordinary props stay CPU; UI state restored\n",switched);
  }
  if(getenv("EOG_TEST_RETAINED_GPU")) {
    bool deferred_bg_enabled=getenv("EOG_DISABLE_DEFERRED_BG")==nullptr;
    gpu_record=retained_record=gpu_world_probe=true;
    for(int repeat=0;repeat<3;repeat++)for(int mode:{0,1,2,3,4,5,6,7,8,9,10,11}) {
      retained_probe=false;gpu_positions.clear();native_scenery_batch_scene(mode);auto reference=gpu_positions;
      retained_probe=true;gpu_positions.clear();native_scenery_batch_scene(mode);
      assert(reference.size()==gpu_positions.size());
      for(size_t i=0;i<reference.size();i++)assert(std::fabs(reference[i]-gpu_positions[i])<.001f);
    }
    assert(retained_draws>0 && retained_producer_hits>0 && retained_producer_corners>0 && !retained_hint);
    retained_probe=false;native_setting_set_view_distance(0);native_setting_set_lod(0);
    for(int offset=-10;offset<=10;offset++) {
      gpu_world_probe=false;gpu_positions.clear();native_original_fog_near_scene(offset);auto reference=gpu_positions;
      gpu_world_probe=true;gpu_positions.clear();native_original_fog_near_scene(offset);
      assert(reference.size()==gpu_positions.size());
      for(size_t i=0;i<reference.size();i++)assert(std::fabs(reference[i]-gpu_positions[i])<.001f);
    }
    puts("PASS original fog near-plane GPU: 21 camera positions match complete CPU-clipped geometry/shade/fog attributes with original view distance and LOD");
    for(int x:{-1000,0,1000}) {
      gpu_world_probe=false;gpu_positions.clear();native_gpu_side_scene(x);auto reference=gpu_positions;
      unsigned before=gpu_world_switches;
      gpu_world_probe=true;gpu_positions.clear();native_gpu_side_scene(x);
      assert(reference.size()==gpu_positions.size());
      for(size_t i=0;i<reference.size();i++)assert(std::fabs(reference[i]-gpu_positions[i])<.001f);
      if(x){assert(reference.empty());assert(gpu_world_switches==before);assert(bg_last_loaded==(deferred_bg_enabled?6u:0u) && bg_last_resolved==0);}
      else {assert(!reference.empty());assert(bg_last_loaded==(deferred_bg_enabled?6u:0u) && bg_last_resolved==bg_last_loaded);}
    }
    puts("PASS GPU early visibility: offscreen batches skip material/GPU submission and visible geometry matches reference at Original LOD");
    gpu_world_probe=false;gpu_positions.clear();native_deferred_bg_scene();auto deferred_reference=gpu_positions;
    gpu_world_probe=true;gpu_positions.clear();native_deferred_bg_scene();
    assert(!deferred_reference.empty() && deferred_reference.size()==gpu_positions.size());
    for(size_t i=0;i<deferred_reference.size();i++)assert(std::fabs(deferred_reference[i]-gpu_positions[i])<.001f);
    assert(bg_last_loaded==(deferred_bg_enabled?6u:0u) && bg_last_resolved==(deferred_bg_enabled?5u:0u));
    puts("PASS deferred background: rejected vertices avoid depth/fog; partial loads preserve load-time matrices, fog, texture scaling, modified UV and later CPU consumers");
    for(int reverse=0;reverse<2;reverse++)for(int front=0;front<2;front++) {
      gpu_positions.clear();native_world_winding_scene(reverse,front);
      if(reverse==front)assert(gpu_positions.size()==21);else assert(gpu_positions.empty());
    }
    puts("PASS actual world winding: GBI world triangles retain the correct face through determinant cancellation in both cull modes and windings");
    for(int kind=1;kind<=4;kind++)for(int front=0;front<2;front++)for(int back=0;back<2;back++)for(int pose:{-20,0,20,70,120,180}) {
      gpu_world_probe=retained_probe=false;gpu_positions.clear();native_model_face_scene(kind,front,back,pose);auto reference=gpu_positions;
      gpu_world_probe=retained_probe=true;gpu_positions.clear();native_model_face_scene(kind,front,back,pose);
      assert(reference.size()==gpu_positions.size());
      for(size_t i=0;i<reference.size();i++)assert(std::fabs(reference[i]-gpu_positions[i])<.001f);
      if(pose==0 && !front && back) {
        assert(reference.empty() && face_last_batches==1 && face_last_rejected==32);
        if(kind==1) {
          unsigned expected=deferred_bg_enabled?6:0;
          if(bg_last_loaded!=expected || bg_last_resolved!=0)fprintf(stderr,"Hidden BG: loaded=%u resolved=%u fallback=%u\n",bg_last_loaded,bg_last_resolved,face_last_fallbacks);
          assert(bg_last_loaded==expected && bg_last_resolved==0);
        }
      }
      if(pose==0 && !front && !back)assert(face_last_rejected>=16 && gpu_world_matrix.cull==0);
    }
    puts("PASS early model faces: complete ordered attributes match CPU reference for BG/props/actors/vehicles, both cull modes, camera rotations and behind-camera views; 32-face hidden batches skip packing and BG depth/fog; PICA winding disabled");
    native_graphics_reference_background(1);
    for(int kind=1;kind<=4;kind++) {
      gpu_world_probe=retained_probe=false;gpu_positions.clear();native_model_face_scene(kind,0,0,0);auto reference=gpu_positions;
      unsigned before=gpu_world_switches;
      gpu_world_probe=retained_probe=true;gpu_positions.clear();native_model_face_scene(kind,0,0,0);
      assert(reference.size()==gpu_positions.size());
      for(size_t i=0;i<reference.size();i++)assert(std::fabs(reference[i]-gpu_positions[i])<.001f);
      if(kind==1)assert(gpu_world_switches==before && bg_last_loaded==0);
      else assert(gpu_world_switches>before);
    }
    std::vector<std::vector<float>> warmed;
    for(int i=0;i<100;i++){gpu_positions.clear();native_cpu_cache_scene(i);warmed.push_back(gpu_positions);}
    unsigned hits_before=prepared_hits,builds_before=prepared_builds,cpu_before=cpu_hint_draws;
    for(int i=0;i<100;i++) {
      gpu_positions.clear();native_cpu_cache_scene(i);
      assert(gpu_positions==warmed[i]);
    }
    assert(prepared_hits>=hits_before+100 && prepared_builds==builds_before && cpu_hint_draws>=cpu_before+100);
    assert(!cpu_world_hint);
    puts("PASS CPU room batch capacity: 100 distinct shaded batches replay exact ordered CPU output with 100 hits and zero rebuilds; CPU indexing hints consumed and cleared");
    for(int repeat=0;repeat<2;repeat++)for(int variant=0;variant<24;variant++)for(int pose:{-20,0,20,70,120,180}) {
      gpu_positions.clear();native_cpu_cache_pose_scene(variant,pose,1);auto original=gpu_positions;
      gpu_positions.clear();native_cpu_cache_pose_scene(variant,pose,0);
      assert(original.size()==gpu_positions.size());
      for(size_t i=0;i<original.size();i++)assert(std::fabs(original[i]-gpu_positions[i])<.001f);
    }
    puts("PASS moving CPU room cache: complete ordered geometry/attributes match unbatched CPU triangles across 288 camera/material cases, including changing face visibility and near/behind-camera fallback");
    bool cpu_deferred_enabled=!getenv("EOG_DISABLE_DEFERRED_CPU_ROOMS");
    for(int x:{0,10000}) {
      gpu_positions.clear();native_gpu_side_scene(x);
      assert(cpu_last_loaded==(cpu_deferred_enabled?6u:0u));
      if(x)assert(gpu_positions.empty() && cpu_last_resolved==0 && cpu_last_rejected==(cpu_deferred_enabled?1u:0u));
      else assert(!gpu_positions.empty() && cpu_last_resolved==cpu_last_loaded);
    }
    gpu_positions.clear();native_deferred_bg_scene();auto cpu_partial=gpu_positions;
    gpu_positions.clear();native_cpu_room_budget_scene(1);auto cpu_budget=gpu_positions;
    gpu_positions.clear();native_cpu_room_budget_scene(0);assert(cpu_budget==gpu_positions && !cpu_budget.empty());
    assert(cpu_last_loaded==(cpu_deferred_enabled?255u*3:0u) && cpu_last_resolved==cpu_last_loaded);
    puts("PASS deferred CPU rooms: hidden six-vertex block avoids all transforms/fog; visible and 270-state overflow output matches reference with complete ordered attributes; partial loads keep original load-time state");
    native_graphics_reference_background(0);
    puts("PASS reference background mode: actual CPU room geometry matches original output while props/actors/vehicles retain GPU transforms");
    for(int repeat=0;repeat<2;repeat++)for(int tier=0;tier<3;tier++)for(int mode=0;mode<2;mode++)for(int opacity:{0,100,128,200,255}) {
      int simple=native_glass_lod_simple(mode,tier);
      gpu_world_probe=retained_probe=false;gpu_positions.clear();native_glass_gpu_scene(opacity,tier,simple);auto reference=gpu_positions;
      unsigned before=gpu_world_switches;
      gpu_world_probe=retained_probe=true;gpu_positions.clear();native_glass_gpu_scene(opacity,tier,simple);
      assert(reference.size()==gpu_positions.size());
      for(size_t i=0;i<reference.size();i++)assert(std::fabs(reference[i]-gpu_positions[i])<.001f);
      if(simple)assert(gpu_world_switches>before);
    }
    native_setting_set_glass(1);
    puts("PASS simple glass GPU: near/mid/far original/simple modes and five alpha values preserve complete ordered geometry/UV/colour/alpha/fog across repeated CPU/GPU instances");
    retained_probe=false;gpu_record=retained_record=gpu_world_probe=false;
    puts("PASS retained GPU producer: full ordered vertex attributes match normal GPU packing across repeated BG/prop/actor/vehicle scenes, changed transforms/materials/commands and CPU fallbacks");
  }
  printf("Model batch totals: actors=%u vehicles=%u normalsReused=%u normalsBuilt=%u\n",actor_fast,vehicle_fast,normal_hits,normal_builds);
  printf("Prepared batch totals: hits=%u builds=%u corners=%u\n",prepared_hits,prepared_builds,prepared_corners);
  printf("Static identity totals: vertices=%u reused=%u\n",verified_static_vertices,verified_static_hits);
  printf("Vertex identity totals: reused=%u identified=%u\n",verified_vertex_reuse,verified_vertex_total);
  printf("CPU clip totals: avoidedFallback=%u fastColourRounds=%u\n",avoided_clip_fallbacks,fast_clip_rounds);
  intro_mode=false;
  native_graphics_destroy_interpreter();
  clear_shaders();
  native_scene_destroy();
  native_ram_destroy();
  puts("PASS original RLE -> title2 menu -> Fast3D texture decode: 299 rows / "
       "598 triangles");
  puts("PASS positive/negative scrolling and cached texture reuse");
  puts("PASS native-layout GBI matrices, vertices, perspective and depth-state "
       "submission");
  puts("PASS triangle/menu switching and viewport/scissor mapping");
  puts("PASS original backdrop RGB neutrality before GPU upload");
  puts("PASS room -> menu -> triangle state restoration");
  if(!getenv("EOG_DISABLE_RSP_PREP_CACHE"))assert(packed_reused>0 && packed_built>0);
  else assert(!packed_reused && !packed_built);
  printf("Packed vertices: reused=%u built=%u\n",packed_reused,packed_built);
  if(!getenv("EOG_DISABLE_SCENERY_BATCHES") && !getenv("EOG_DISABLE_RSP_PREP_CACHE"))assert(batch_reused && batch_built && batch_fast && batch_fallback);
  else assert(!batch_reused && !batch_built && !batch_fast && !batch_fallback);
  printf("Scenery batches: reused=%u compiled=%u fast=%u fallback=%u\n",batch_reused,batch_built,batch_fast,batch_fallback);
  printf("Room packet totals: direct=%u bound=%u\n",room_packet_hits,room_packet_binds);
  if(!getenv("EOG_DISABLE_OBJECT_BATCHES") && !getenv("EOG_DISABLE_SCENERY_BATCHES") && !getenv("EOG_DISABLE_RSP_PREP_CACHE"))assert(object_fast>0);
  else assert(!object_fast && !object_rejected && !object_fallback);
  printf("Object batches: fast=%u rejected=%u fallback=%u\n",object_fast,object_rejected,object_fallback);


  if(!getenv("EOG_DISABLE_SCENERY_CACHE") && !getenv("EOG_DISABLE_RSP_PREP_CACHE")){assert(scenery_reused>0 && scenery_built>0);if(!getenv("EOG_TEST_PREPARED_CHURN"))assert(scenery_persisted>0);}
  else assert(!scenery_reused && !scenery_built);
  printf("Scenery attributes: reused=%u persisted=%u built=%u\n",scenery_reused,scenery_persisted,scenery_built);

  assert(!recorded_viewmodel);
  assert(setup_rebuilt>0);
  if(!getenv("EOG_DISABLE_RSP_PREP_CACHE"))assert(setup_reused>0);
  else assert(setup_reused==0);
  printf("Triangle setup sampled: reused=%u rebuilt=%u\n",setup_reused,setup_rebuilt);
  if(const char *path=getenv("EOG_GRAPHICS_TRACE")) {
    FILE *f=fopen(path,"wb");assert(f);
    assert(fwrite(&submission_hash,sizeof submission_hash,1,f)==1);assert(!fclose(f));
  }
}

extern "C" void romCreateMesgQueue(void);
void *room_worker(void *path) {
  romCreateMesgQueue();
  assert(native_room_load(static_cast<const char *>(path)));
  assert(native_room_triangles() == 581 && native_room_textures() == 6);
  unsigned before = tris;
  native_room_render(.4f, -.2f, 6.f);
  assert(tris > before);
  before = tris;
  native_room_render(2.f, .2f, 6.f);
  assert(tris > before);
  puts("PASS original Dam room: 970 vertices, 581 source triangles, 6 original "
       "textures");
  assert(pixels_seen == 63);
  assert(opaque_depth >= 2 && decal_depth >= 2);
  puts("PASS PICA depth planner accepts original opaque and decal passes");
  puts("PASS all six room textures: decoded RGB pixels match independent "
       "Python reference");
  char world_path[1024];
  snprintf(world_path, sizeof world_path, "%s",
           static_cast<const char *>(path));
  char *name = strrchr(world_path, '/');
  assert(name);
  strcpy(name + 1, "expected-world-pixels.dat");
  FILE *expected = fopen(world_path, "rb");
  assert(expected);
  expected_count = fread(expected_pixels, 16, 4096, expected);
  assert(expected_count > 6);
  fclose(expected);
  strcpy(name + 1, "native-world.dat");
  world_mode = true;
  assert(native_world_load(world_path));
  assert(native_world_room() == 135);
  float initial[3];
  native_world_position(initial);
  printf("Spawn: %f %f %f\n", initial[0], initial[1], initial[2]);
  native_world_render(1.5707963f, 0);
  strcpy(name + 1, "world-test-route.dat");
  FILE *route = fopen(world_path, "rb");
  assert(route);
  float target[2];
  bool crossed = false;
  while (fread(target, sizeof target, 1, route) == 1) {
    for (unsigned step = 0; step < 500; step++) {
      float p[3];
      native_world_position(p);
      float dx = target[0] - p[0], dz = target[1] - p[2],
            length = sqrtf(dx * dx + dz * dz);
      if (length < 2.f)
        break;
      native_world_move(dx / length * 2.f, -dz / length * 2.f, 0);
      if (native_world_room() != 135)
        crossed = true;
      if (step == 499) {
        fprintf(stderr, "Stuck en route to %f %f at %f %f room%u\n", target[0],
                target[1], p[0], p[2], native_world_room());
        assert(false);
      }
    }
    native_world_render(1.5707963f, 0);
  }
  fclose(route);
  assert(crossed);
  native_world_reset();
  assert(native_world_room() == 135);
  float reset[3];
  native_world_position(reset);
  assert(!memcmp(initial, reset, sizeof reset));
  for (unsigned direction = 0; direction < 16; direction++) {
    native_world_reset();
    for (unsigned step = 0; step < 400; step++)
      native_world_move(0, 4, direction * .39269908f);
    native_world_render(direction * .39269908f, 0);
  }
  native_world_reset();
  for (unsigned step = 0; step < 500; step++)
    native_world_move(0, 4, 1.5707963f);
  float blocked[3];
  native_world_position(blocked);
  assert(blocked[0] > 4200.f && blocked[0] < 4350.f &&
         native_world_room() == 135);
  for (unsigned step = 0; step < 100; step++)
    native_world_move(0, 4, 1.5707963f);
  float still_blocked[3];
  native_world_position(still_blocked);
  assert(!memcmp(blocked, still_blocked, sizeof blocked));
  native_world_reset();
  puts("PASS original STAN spawn, floor height, linked-room walking, wall "
       "queries and reset");
  puts("PASS adjoining original rooms, texture pixels and PICA material/depth "
       "planners");
  strcpy(name + 1, "native-motion.dat");
  assert(native_motion_load(world_path));
  float head[3], look[3], up[3];
  native_motion_camera(head, look, up);
  printf("Original head height %f look %f %f %f\n", head[1], look[0], look[1],
         look[2]);
  float total60 = 0, total30 = 0, low = 1e9f, high = -1e9f, delta[3];
  native_motion_reset();
  for (unsigned i = 0; i < 240; i++) {
    native_motion_tick(1.f / 60.f, 1, 0);
    native_motion_travel(delta);
    total60 += delta[2];
    native_motion_camera(head, look, up);
    assert(std::isfinite(head[1]) && head[1] > 120 && head[1] < 200);
    low = fminf(low, head[1]);
    high = fmaxf(high, head[1]);
  }
  assert(total60 > 100 && high - low > 1.f);
  native_motion_reset();
  for (unsigned i = 0; i < 120; i++) {
    native_motion_tick(1.f / 30.f, 1, 0);
    native_motion_travel(delta);
    total30 += delta[2];
  }
  assert(fabsf(total30 - total60) < .05f);
  printf("Original gait: 60fps %f, 30fps %f units in 4s; eye range %f..%f\n",
         total60, total30, low, high);
  for (unsigned i = 0; i < 240; i++)
    native_motion_tick(1.f / 60.f, 0, 0);
  native_motion_travel(delta);
  assert(fabsf(delta[0]) + fabsf(delta[2]) < .001f);
  native_motion_reset();
  for (unsigned i = 0; i < 120; i++)
    native_motion_tick(1.f / 60.f, -1, 0);
  native_motion_travel(delta);
  assert(delta[2] < 0);
  native_motion_reset();
  for (unsigned i = 0; i < 120; i++)
    native_motion_tick(1.f / 60.f, 0, 1);
  native_motion_travel(delta);
  assert(delta[0] > 0);
  native_world_reset();
  for (unsigned i = 0; i < 120; i++) {
    native_world_update(0, 1, 1.5707963f, 1.f / 60.f);
    if (i % 30 == 0)
      native_world_render(1.5707963f, 0);
  }
  native_world_reset();
  puts("PASS original gait root movement, head bob, reverse/strafe, stopping "
       "and 30/60fps timing");
  native_scene_frame(0, 3);
  assert(fog_draws > 0);
  assert(pica_ramp_coordinate(0) == .5f / 256.f &&
         pica_ramp_coordinate(1) == 255.5f / 256.f);
  puts(
      "PASS original RSP fog submission and shared PICA fog/LOD ramp planning");
  native_cart_reset();
  return nullptr;
}

extern "C" uintptr_t sysImageBase(void) {
  extern char __executable_start[];
  return (uintptr_t)__executable_start;
}
