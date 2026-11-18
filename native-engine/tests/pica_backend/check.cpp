/* Record actual old/new backend GPU submissions against real Citro3D data
 * types and inline TEV builders. This does not emulate rasterization. */
#include <3ds.h>
#include <citro3d.h>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>
#include <map>
#include "native_gpu_bounds.h"
#ifdef BACKEND_REFERENCE
#include "reference/gfx_pica_before.cpp"
static unsigned merged_draws,material_hits,material_builds;
#else
#include "../../../src/gfx_pica.cpp"
#endif
struct RecordedState {
 C3D_TexEnv env[6];uintptr_t textures[3];u32 texture_params[3];u32 viewport[4],scissor[4];
 unsigned blend_eq,blend_src,blend_dst;
 uintptr_t target;unsigned depth_compare;bool depth,blend,alpha;int alpha_ref;float depth_scale,depth_bias;unsigned buffer_rgb,buffer_alpha;
};
struct RecordedVertex {float pos[4],colour[4],uv[3][2];unsigned used_uv;RecordedState state;};
static RecordedState driver;
static std::vector<RecordedVertex> recorded;
static void *driver_vertices;
static std::map<const C3D_BufInfo *,const void *> driver_buffers;
static bool driver_current_backend;
static unsigned submissions;
static std::vector<float> draw_uniforms;
#ifndef BACKEND_REFERENCE
static std::vector<float> draw_eye_offsets,draw_world_depth;
#endif
static GPU_CULLMODE driver_cull=GPU_CULL_NONE;
static shaderProgram_s *driver_program;
#ifndef BACKEND_REFERENCE
static bool watch_owner,allow_watch=true;static int watch_regions[5][4],watch_clears;
extern "C" int native_setting_watch(void){return 0;}
extern "C" int native_bottom_gpu_acquire(void){watch_owner=allow_watch;return allow_watch;}
extern "C" void native_bottom_gpu_release(void){watch_owner=false;}
extern "C" void native_bottom_watch_regions(const int bounds[5][4]){memcpy(watch_regions,bounds,sizeof watch_regions);}
#endif
Result GSPGPU_FlushDataCache(const void *,u32){return 0;}
#ifndef BACKEND_REFERENCE
static unsigned depth_copies,depth_splits,depth_invalidations;
Result GSPGPU_InvalidateDataCache(const void *,u32){depth_invalidations++;return 0;}
Result GX_TextureCopy(u32 *src,u32 in,u32 *dst,u32 out,u32 bytes,u32 flags){
 assert(in==out && flags==BIT(3) && bytes<=DEPTH_COPY_CAPACITY && depth_splits>depth_copies);
 memcpy(dst,src,bytes);depth_copies++;return 0;
}
#endif

extern "C" {
C3D_FVec C3D_FVUnif[2][C3D_FVUNIF_COUNT];
bool C3D_FVUnifDirty[2][C3D_FVUNIF_COUNT];
u16 C3D_BoolUnifs[2];bool C3D_BoolUnifsDirty[2];
void sysLogPrintf(int,const char *,...){}
void sysFatalError(const char *format,...) {
 va_list args;va_start(args,format);vfprintf(stderr,format,args);va_end(args);abort();
}
C3D_TexEnv *C3D_GetTexEnv(int index){return &driver.env[index];}
void C3D_SetTexEnv(int index,C3D_TexEnv *env){driver.env[index]=*env;}
void C3D_TexEnvBufUpdate(int mode,int mask){if(mode&C3D_RGB)driver.buffer_rgb=mask;if(mode&C3D_Alpha)driver.buffer_alpha=mask;}
void C3D_TexBind(int unit,C3D_Tex *tex){driver.textures[unit]=(uintptr_t)tex->data;driver.texture_params[unit]=tex->param;}
void C3D_CullFace(GPU_CULLMODE mode){driver_cull=mode;}
void C3D_BindProgram(shaderProgram_s *p){driver_program=p;}
void C3D_AlphaTest(bool enabled,GPU_TESTFUNC,int ref){driver.alpha=enabled;driver.alpha_ref=ref;}
void C3D_DepthTest(bool enabled,GPU_TESTFUNC compare,GPU_WRITEMASK){driver.depth=enabled;driver.depth_compare=compare;}
void C3D_DepthMap(bool,float scale,float bias){driver.depth_scale=scale;driver.depth_bias=bias;}
void C3D_AlphaBlend(GPU_BLENDEQUATION eq,GPU_BLENDEQUATION,GPU_BLENDFACTOR src,GPU_BLENDFACTOR dst,GPU_BLENDFACTOR,GPU_BLENDFACTOR){driver.blend=src==GPU_SRC_ALPHA;driver.blend_eq=eq;driver.blend_src=src;driver.blend_dst=dst;}
void C3D_SetViewport(u32 x,u32 y,u32 w,u32 h){driver.viewport[0]=x;driver.viewport[1]=y;driver.viewport[2]=w;driver.viewport[3]=h;}
void C3D_SetScissor(GPU_SCISSORMODE,u32 x,u32 y,u32 w,u32 h){driver.scissor[0]=x;driver.scissor[1]=y;driver.scissor[2]=w;driver.scissor[3]=h;}
bool C3D_FrameDrawOn(C3D_RenderTarget *target){driver.target=(uintptr_t)target;return true;}
void C3D_SetBufInfo(C3D_BufInfo *info){
#ifndef BACKEND_REFERENCE
 if(auto it=driver_buffers.find(info);it!=driver_buffers.end()){driver_vertices=const_cast<void *>(it->second);return;}
 driver_vertices=info==&watch_buffer?watch_vertices:info==&sharp_buffer?sharp_vertices:vertices;
#else
 (void)info;
#endif
}
#ifndef BACKEND_REFERENCE
void BufInfo_Init(C3D_BufInfo *b){*b={};}
int BufInfo_Add(C3D_BufInfo *b,const void *p,ptrdiff_t,int,u64){driver_buffers[b]=p;return b->bufCount++;}
static C3D_RenderTarget *outputs[3];
static int fail_create,live_targets,live_sharp_textures;
static int test_vignette_mode;static unsigned test_render_state;
int native_setting_vignette_mode(void){return test_vignette_mode;}
unsigned native_setting_render_state(void){return test_render_state;}
static int test_sharpness,test_vignette,test_bloom,test_grain,test_aa=1;
int native_setting_grain(void){return test_grain;}
void native_setting_set_grain(int v){test_grain=v;}
int native_setting_vignette(void){return test_vignette;}
int native_setting_bloom(void){return test_bloom;}
void native_setting_set_vignette(int v){test_vignette=v;}
void native_setting_set_bloom(int v){test_bloom=v;}
int native_setting_stereo_strength(void){return 100;}
static int test_stereo_auto=1,test_stereo_convergence=100,test_stereo_comfort=0,test_stereo_hud=75,test_stereo_crosshair=0,test_ghost_contrast=100,test_black_floor;
int native_setting_stereo_auto(void){return test_stereo_auto;}
int native_setting_stereo_convergence(void){return test_stereo_convergence;}
int native_setting_stereo_comfort(void){return test_stereo_comfort;}
int native_setting_stereo_hud_depth(void){return test_stereo_hud;}
int native_setting_stereo_crosshair_hud(void){return test_stereo_crosshair;}
int native_setting_stereo_ghost_contrast(void){return test_ghost_contrast;}
int native_setting_stereo_black_floor(void){return test_black_floor;}
void native_setting_set_stereo_ghost_contrast(int v){test_ghost_contrast=v;}
void native_setting_set_stereo_black_floor(int v){test_black_floor=v;}
static u32 ghost_blend_colour;
void C3D_BlendingColor(u32 colour){ghost_blend_colour=colour;}

int native_setting_aa(void){return test_aa;}
void native_setting_set_aa(int value){test_aa=value;}
int native_setting_sharpness(void){return test_sharpness;}
void native_setting_set_sharpness(int value){test_sharpness=value;}
C3D_RenderTarget *C3D_RenderTargetCreate(int w,int h,GPU_COLORBUF f,C3D_DEPTHTYPE) {
 assert(!in_frame);
 if(fail_create && !--fail_create)return nullptr;
 auto *t=static_cast<C3D_RenderTarget *>(calloc(1,sizeof(C3D_RenderTarget)));assert(t);
 t->frameBuf.width=w;t->frameBuf.height=h;t->frameBuf.colorFmt=f;live_targets++;return t;
}
void C3D_RenderTargetDelete(C3D_RenderTarget *t){assert(!in_frame && t);if(t->linked)C3D_RenderTargetDetachOutput(t);free(t);live_targets--;}
bool C3D_TexInitWithParams(C3D_Tex *t,C3D_TexCube *,C3D_TexInitParams p) {
 assert(!p.onVram || !in_frame);*t={};t->data=calloc(1,C3D_TexCalcTotalSize(p.width*p.height*4,p.maxLevel));assert(t->data);
 t->width=p.width;t->height=p.height;t->fmt=p.format;t->size=p.width*p.height*4;t->maxLevel=p.maxLevel;live_sharp_textures++;return true;
}
void C3D_TexGenerateMipmap(C3D_Tex *t,GPU_TEXFACE){assert(t->data && t->maxLevel);}
void C3D_TexFlush(C3D_Tex *){}
void C3D_TexDelete(C3D_Tex *t){free(t->data);t->data=nullptr;live_sharp_textures--;}
C3D_RenderTarget *C3D_RenderTargetCreateFromTex(C3D_Tex *t,GPU_TEXFACE,int,C3D_DEPTHTYPE depth) {
 auto *rt=C3D_RenderTargetCreate(t->width,t->height,GPU_RB_RGBA8,depth);if(rt)rt->frameBuf.colorBuf=t->data;return rt;
}
void C3D_RenderTargetSetOutput(C3D_RenderTarget *t,gfxScreen_t screen,gfx3dSide_t side,u32 flags){
 unsigned id=screen==GFX_BOTTOM?2:side==GFX_RIGHT?1:0;
 if(outputs[id])outputs[id]->linked=false;
 outputs[id]=t;
 if(t){t->screen=screen;t->side=side;t->linked=true;t->transferFlags=flags;
   if(screen==GFX_BOTTOM)assert(flags==(GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565)));
 }
}
void C3D_FrameBufClear(C3D_FrameBuf *,C3D_ClearBits,u32,u32){watch_clears++;}
void C3D_FrameSplit(u8){depth_splits++;}
#endif
static void record_vertex(int i) {
 auto *data=static_cast<Vertex *>(driver_vertices);
   RecordedVertex out={};out.state=driver;
   memcpy(out.pos,data[i].pos,sizeof out.pos);memcpy(out.colour,data[i].color,sizeof out.colour);
#ifndef BACKEND_REFERENCE
   if(world_palette_active) {
     // Emulate the world shader's relative-addressed rigid transform. Read
     // uniforms at submission time so premature palette reuse corrupts tests.
     unsigned row=unsigned(out.pos[3]);assert(row%4==0 && row<WORLD_PALETTE_SIZE*4);
     float position[4]={out.pos[0],out.pos[1],out.pos[2],1};
     for(unsigned j=0;j<4;j++) {
       const auto &m=C3D_FVUnif[0][world_matrix_uniform+row+j];
       out.pos[j]=m.x*position[0]+m.y*position[1]+m.z*position[2]+m.w;
     }
     // Canonical N64 clip depth for existing CPU/reference comparisons.
     out.pos[2]=2*out.pos[2]+out.pos[3];
   }
   else if(!gpu_world)out.pos[2]=2*out.pos[2]+out.pos[3];
#endif
   memcpy(out.uv,data[i].uv,sizeof out.uv);
   for(const auto &env:driver.env)for(unsigned channel=0;channel<2;channel++)for(unsigned arg=0;arg<3;arg++) {
     unsigned src=((channel?env.srcAlpha:env.srcRgb)>>(arg*4))&15;
     if(src>=GPU_TEXTURE0 && src<=GPU_TEXTURE2)out.used_uv|=1u<<(src-GPU_TEXTURE0);
   }
   recorded.push_back(out);
}
void C3D_DrawArrays(GPU_Primitive_t,int first,int count) {
 submissions++;draw_uniforms.push_back(C3D_FVUnif[0][0].x);
#ifndef BACKEND_REFERENCE
 draw_eye_offsets.push_back(C3D_FVUnif[0][world_palette_active?world_stereo_uniform:clip_stereo_uniform].y);
 draw_world_depth.push_back(C3D_FVUnif[0][world_palette_active?world_stereo_uniform:clip_stereo_uniform].z);
#endif
 for(int i=first;i<first+count;i++)record_vertex(i);
}
void C3D_DrawElements(GPU_Primitive_t,int count,int type,const void *indices) {
 assert(type==C3D_UNSIGNED_SHORT && indices);
 submissions++;draw_uniforms.push_back(C3D_FVUnif[0][0].x);
#ifndef BACKEND_REFERENCE
 draw_eye_offsets.push_back(C3D_FVUnif[0][world_palette_active?world_stereo_uniform:clip_stereo_uniform].y);
 draw_world_depth.push_back(C3D_FVUnif[0][world_palette_active?world_stereo_uniform:clip_stereo_uniform].z);
#endif
 auto *elements=static_cast<const uint16_t *>(indices);
 for(int i=0;i<count;i++)record_vertex(elements[i]);
}
}
static std::vector<float> make_vertices(const CCFeatures &c,float constant,bool scalar=false) {
 unsigned stride=4+(c.opt_fog?4:0)+(c.opt_alpha?4:3)*c.num_inputs;
 for(unsigned t=0;t<2;t++)if(c.used_textures[t])stride+=2+c.clamp[t][0]+c.clamp[t][1];
 std::vector<float> v(stride*6);
 for(unsigned i=0;i<6;i++) {
  float *p=v.data()+i*stride;*p++=.1f*i;*p++=.2f;*p++=.3f;*p++=1;
  for(unsigned t=0;t<2;t++)if(c.used_textures[t]){
   *p++=.2f+i*.02f;*p++=.9f-i*.03f;
   if(c.clamp[t][0])*p++=.6f;
   if(c.clamp[t][1])*p++=.8f;
  }
  if(c.opt_fog){*p++=.1f;*p++=.2f;*p++=.3f;*p++=.15f*i;}
  for(unsigned k=0;k<unsigned(c.num_inputs);k++) {
   bool grey=scalar || (c.num_inputs==3 && k==0);
   float fixed=c.num_inputs==3?1.f:constant;
   *p++=k==1?fixed:.3f+i*.01f;
   *p++=k==1?fixed:grey?.3f+i*.01f:.7f;
   *p++=k==1?fixed:grey?.3f+i*.01f:.5f;
   if(c.opt_alpha)*p++=k==1?(c.num_inputs==3?1.f:.6f):grey?1.f:.8f;
  }
 }
 return v;
}
static void barrier() {
#ifndef BACKEND_REFERENCE
 flush_draw();
#endif
}
static void run_material(uint64_t id,uint32_t flags,bool scalar=false) {
 ShaderProgram shader={};shader.id0=id;shader.id1=flags;shader.colors_logged=true;
 gfx_cc_get_features(id,flags,&shader.cc);active=&shader;
 for(unsigned i=0;i<12;i++) {
   if(i==3)depth(true,true,true,false,0);
   if(i==5)viewport(40,0,320,240);
   if(i==7)scissor(40,20,320,200);
   if(i==9)alpha(true,false);
   auto data=make_vertices(shader.cc,i<6?.4f:.7f,scalar);
   // Alternate textures without a geometry reorder; repeating runs merge.
   selected[0]=i<4?1:2;selected[1]=i<8?3:4;
#ifndef BACKEND_REFERENCE
   const float rgb[3]={.1f,.2f,.3f};
   native_pica_fog_hint(shader.cc.opt_fog && i%2,rgb);
#endif
   draw(data.data(),data.size(),2);
#ifndef BACKEND_REFERENCE
   native_pica_fog_hint(0,nullptr);
#endif
 }
 barrier();
}
int main(int argc,char **argv) {
#ifndef BACKEND_REFERENCE
 clip_stereo_uniform=4;world_stereo_uniform=88;
#endif

 assert(argc==2);
#ifdef BACKEND_REFERENCE
 driver_current_backend=false;
#else
 driver_current_backend=true;
#endif
 frame_count=1;in_frame=true;
 vertices=static_cast<Vertex *>(calloc(MAX_VERTICES,sizeof(Vertex)));assert(vertices);
 driver_vertices=vertices;
 scalar_ramp.data=reinterpret_cast<void *>(1004);
 noise_samples[0].data=reinterpret_cast<void *>(1005);noise_samples[1].data=reinterpret_cast<void *>(1006);
 for(unsigned i=0;i<4;i++) {
   auto *t=new Texture{};t->ready=true;t->width=i%2?61:32;t->height=i%2?29:32;
   t->gpu.width=t->gpu.height=64;t->gpu.data=reinterpret_cast<void *>(100+i);
   textures.push_back(t);
 }
 run_material(0x1000,0); // shaded opaque
 run_material(0x10001000,1); // shaded alpha
 run_material(0xd000020d210c818aULL,0x17); // crates + fog
 run_material(0xd000030d12181218ULL,0x13); // gunbarrel two-cycle
 run_material(0x1081000,9); // textured watch noise
 run_material(0x10001000,9); // untextured fade noise
 run_material(0x10000a08,9); // two textures: screen noise occupies unit 2
 run_material(0x20d020d010c818aULL,0x13,true); // scalar material + fog
 // Revisit an earlier shader after others to exercise non-consecutive cache hits.
 run_material(0xd000020d210c818aULL,0x17);
 barrier();
 FILE *f=fopen(argv[1],driver_current_backend?"rb":"wb");assert(f);
 if(!driver_current_backend) {
   assert(fwrite(recorded.data(),sizeof(RecordedVertex),recorded.size(),f)==recorded.size());
   printf("Reference backend: %zu vertices, %u GPU submissions\n",recorded.size(),submissions);
 } else {
   for(size_t i=0;i<recorded.size();i++) {
     RecordedVertex old={};assert(fread(&old,sizeof old,1,f)==1);const auto &current=recorded[i];
     // The reference used GEQUAL for every material. Opaque/XLU now match
     // upstream GL_LESS (reversed GREATER); all other recorded state is exact.
     if(old.state.depth_compare!=current.state.depth_compare) {
       assert(old.state.depth_compare==GPU_GEQUAL && current.state.depth_compare==GPU_GREATER);
       old.state.depth_compare=current.state.depth_compare;
     }
     if(memcmp(&old.state,&current.state,sizeof old.state)) {
       fprintf(stderr,"GPU state differs at vertex %zu\n",i);abort();
     }
     assert(!memcmp(old.pos,current.pos,sizeof old.pos));assert(!memcmp(old.colour,current.colour,sizeof old.colour));
     assert(old.used_uv==current.used_uv);
     assert(!memcmp(old.uv,current.uv,sizeof old.uv));
   }
   assert(fgetc(f)==EOF);assert(merged_draws>0 && material_hits>material_builds);
   printf("PASS actual PICA backend: %zu ordered vertices and GPU state match reference except corrected opaque depth comparison; %u GPU submissions, %u merged draws, cache hits=%u builds=%u\n",recorded.size(),submissions,merged_draws,material_hits,material_builds);
 }
 fclose(f);
#ifndef BACKEND_REFERENCE
 // More than four/eight meshes can share a hash bucket without forcing
 // eviction. Exercise actual persistent GPU cache admission across frames.
 {
   PicaRetainedMesh cache;std::vector<uint64_t> tokens;
   for(uint64_t t=1;tokens.size()<100;t++) {
     uint64_t h=t;h^=h>>33;
     if((unsigned(h^(h>>32))%64)==0)tokens.push_back(t);
   }
   Vertex mesh[3]={};uint16_t indices[3]={0,1,2};
   for(unsigned frame=0;frame<9;frame++) {
     cache.begin_frame();
     for(auto token:tokens) {
       PicaRetainedMesh::Key key={};key.token=token;key.layout.count=3;key.layout.stride=7;
       if(!cache.find(key))cache.store(key,mesh,3,indices,0);
     }
     if(frame==8)assert(cache.hits==100 && cache.misses==0);
   }
   cache.clear();
   NativeRetainedIndex<256,64> index;unsigned keys[256]={};
   for(unsigned i=0;i<256;i++){unsigned slot=index.victim();keys[slot]=i;index.install(slot,i*64);}
   for(unsigned i=0;i<256;i++)assert(index.find(i*64,[&](unsigned slot){return keys[slot]==i;})==int(i));
   index.touch(0);unsigned slot=index.victim();assert(slot==1);keys[slot]=256;index.install(slot,256*64);
   assert(index.find(64,[&](unsigned s){return keys[s]==1;})<0);
   assert(index.find(0,[&](unsigned s){return keys[s]==0;})==0);
   index.clear();assert(index.victim()==0 && index.find(0,[](unsigned){return true;})<0);
   static_assert(sizeof index<4096);
   puts("PASS retained collision reuse: 100 colliding GPU meshes settle to zero misses, full producer index capacity, true LRU replacement and cleanup");
 }
 // World shader switches flush pending geometry before modifying matrix/cull.
 {
   NativeGpuWorld world={};for(unsigned i=0;i<4;i++)world.rows[i][i]=1;
   world.rows[0][3]=12;world.rows[2][3]=-500;world.cull=2;
   clip_tilt_uniform=0;world_tilt_uniform=4;world_matrix_uniform=8;clip_stereo_uniform=4;world_stereo_uniform=88;
   native_pica_gpu_world(&world);
   assert(gpu_world && driver_program==&world_program && driver_cull==GPU_CULL_BACK_CCW);
   assert(C3D_FVUnif[0][8].w==12 && C3D_FVUnif[0][10].w==-250.5f);
   unsigned before=submissions;run_material(0x1000,0);
   native_pica_gpu_world(nullptr);assert(submissions>before);
   assert(!gpu_world && driver_program==&program && driver_cull==GPU_CULL_NONE && tilt_uniform==clip_tilt_uniform);
   world.cull=1;native_pica_gpu_world(&world);assert(driver_cull==GPU_CULL_FRONT_CCW);
   native_pica_gpu_world(nullptr);
   puts("PASS GPU world backend: ordered shader/matrix/cull switches, GPU uniform rows, pending draw flush and reference/UI restoration");
 }
 // Exercise the actual uploaded depth row with float24-sized mantissas.
 // Full float32 shader mocks hide cancellation of distant clip Z and W.
 {
   auto f24=[](float x){uint32_t bits;memcpy(&bits,&x,4);bits&=~127u;memcpy(&x,&bits,4);return x;};
   NativeGpuWorld world={};for(unsigned i=0;i<4;i++)world.rows[i][i]=1;
   world.rows[2][2]=-1.00002f;world.rows[2][3]=-10;
   world.rows[3][2]=-1;world.rows[3][3]=0;
   native_pica_gpu_world(&world);
   const auto depth=C3D_FVUnif[0][world_matrix_uniform+world_palette_row+2];
   unsigned improved=0;
   for(float z:{-5000.f,-10000.f,-20000.f,-30000.f}) {
     double expected=((double(world.rows[2][2])-world.rows[3][2])*z-10)*.5;
     float old=f24(f24(f24(f24(world.rows[2][2])*z)-10)-f24(-z))*.5f;
     float stable=f24(f24(f24(depth.z)*z)+f24(depth.w));
     assert(fabs(stable-expected)<.001);
     if(fabs(old-expected)>.01)improved++;
   }
   assert(improved>=3);native_pica_gpu_world(nullptr);
   puts("PASS GPU world depth precision: actual uploaded depth row avoids distant float24 Z/W cancellation");
   ShaderProgram test={};test.id0=0x1000;gfx_cc_get_features(test.id0,test.id1,&test.cc);active=&test;
   auto source=make_vertices(test.cc,.4f);unsigned stride=source.size()/6;
   for(float w:{5000.f,10000.f,20000.f,30000.f}) {
     float z=w-9.6f;
     for(unsigned i=0;i<6;i++){source[i*stride+2]=z;source[i*stride+3]=w;}
     unsigned start=vertex_count;draw(source.data(),source.size(),2);barrier();
     float expected=(z-w)*.5f;
     for(unsigned i=0;i<6;i++)assert(vertices[start+i].pos[2]==expected);
     assert(fabs(f24(vertices[start].pos[2])-expected)<.001f);
     assert(fabs(f24((f24(z)-f24(w))*.5f)-expected)>.01f);
   }
   puts("PASS unified clip depth: actual CPU fallback vertices preserve depth before float24 upload, matching world depth convention");
 }
 {
   // Independently enumerate transformed box corners. An intersecting box
   // must survive, including camera-plane crossings and tiny plane offsets.
   unsigned rejected=0,kept=0;
   for(float angle:{0.f,.3f,1.2f})for(float z:{-100.f,-1.f,0.f,1.f,100.f})for(float x:{-1000.f,-1.00001f,0.f,1.00001f,1000.f}) {
     float rows[4][4]={{cosf(angle),0,sinf(angle),x},{0,1,0,0},{0,0,-1,-10},{-sinf(angle),0,cosf(angle),z}};
     float lo[3]={-1,-1,-1},hi[3]={1,1,1};bool outside[4]={true,true,true,true};
     for(unsigned c=0;c<8;c++) {
       double clip[4]={};for(unsigned r=0;r<4;r++) {
         clip[r]=rows[r][3];for(unsigned a=0;a<3;a++)clip[r]+=double(rows[r][a])*((c&(1u<<a))?hi[a]:lo[a]);
       }
       outside[0]&=clip[0]<-clip[3];outside[1]&=clip[0]>clip[3];
       outside[2]&=clip[1]<-clip[3];outside[3]&=clip[1]>clip[3];
     }
     if(native_gpu_bounds_outside(rows,lo,hi)){assert(outside[0]||outside[1]||outside[2]||outside[3]);rejected++;}else kept++;
   }
   assert(rejected && kept);
   puts("PASS GPU visibility bounds: conservative side-plane rejection preserves intersecting and camera-crossing boxes");
 }
 // Different rigid parts with identical material share submissions. Exercise
 // both index/array streams, palette rollover, cull barriers and UI restore.
 {
   ShaderProgram test={};test.id0=0x1000;test.id1=0;
   gfx_cc_get_features(test.id0,test.id1,&test.cc);active=&test;
   auto source=make_vertices(test.cc,.4f);unsigned stride=source.size()/6;
   for(unsigned i=3;i<6;i++)memcpy(source.data()+i*stride,source.data()+(i-3)*stride,stride*sizeof(float));
   uint64_t ids[6]={1,2,3,1,2,3};
   auto *elements=static_cast<uint16_t *>(calloc(MAX_VERTICES,sizeof(uint16_t)));
   assert(elements);
   for(bool use_indices:{false,true}) {
     world_indices=use_indices?elements:nullptr;world_index_count=0;
     if(use_indices)native_pica_vertex_ids(source.data(),ids,6);
     size_t start=recorded.size();unsigned before=submissions;
     unsigned continuations=world_palette_continued,rollovers=world_palette_rollovers;
     for(unsigned part=0;part<WORLD_PALETTE_SIZE+3;part++) {
       NativeGpuWorld world={};for(unsigned j=0;j<4;j++)world.rows[j][j]=1;
       world.rows[0][0]=1.f+part*.125f;world.rows[0][3]=part*7.f;
       world.rows[1][3]=-float(part);world.rows[2][3]=-50.f-part;
       world.cull=2;native_pica_gpu_world(&world);
       draw(source.data(),source.size(),2);
     }
     native_pica_gpu_world(nullptr);
     assert(submissions-before==2); // 23 bones, two palette-sized draws.
     assert(world_palette_continued-continuations==WORLD_PALETTE_SIZE+1);
     assert(world_palette_rollovers-rollovers==1);
     assert(recorded.size()-start==(WORLD_PALETTE_SIZE+3)*6);
     for(unsigned part=0;part<WORLD_PALETTE_SIZE+3;part++)for(unsigned i=0;i<6;i++) {
       const auto &out=recorded[start+part*6+i];const float *p=source.data()+i*stride;
       assert(fabsf(out.pos[0]-(p[0]*(1.f+part*.125f)+part*7.f))<.0001f);
       assert(fabsf(out.pos[1]-(p[1]-part))<.0001f);
       assert(fabsf(out.pos[2]-(p[2]-50.f-part))<.0001f);
       assert(out.pos[3]==1);
     }
     NativeGpuWorld world={};for(unsigned j=0;j<4;j++)world.rows[j][j]=1;
     world.cull=2;native_pica_gpu_world(&world);draw(source.data(),source.size(),2);
     before=submissions;world.cull=1;native_pica_gpu_world(&world);assert(submissions==before+1);
     draw(source.data(),source.size(),2);before=submissions;
     native_pica_gpu_world(nullptr);assert(submissions==before+1 && !world_palette_active);
     native_pica_vertex_ids(nullptr,nullptr,0);
   }
   free(elements);world_indices=nullptr;world_index_count=0;
   puts("PASS GPU matrix palette: 23 rigid parts become two ordered draws with exact transforms, indexed/array streams, safe rollover, cull barriers and UI restoration");
 }
 // Compare persistent GPU buffers against the actual conversion path, not
 // another implementation of its math. Fog changes while source stays static.
 {
   mesh_cache.clear();mesh_cache.begin_frame();
   ShaderProgram test={};test.id0=0xd000020d210c818aULL;test.id1=0x17;
   gfx_cc_get_features(test.id0,test.id1,&test.cc);active=&test;
   auto source=make_vertices(test.cc,.4f);unsigned stride=source.size()/6;
   unsigned fog=stride-(test.cc.opt_alpha?4:3)*test.cc.num_inputs-4;
   auto compare=[&]() {
     gpu_world=false;size_t start=recorded.size();draw(source.data(),source.size(),2);barrier();
     std::vector<RecordedVertex> expected(recorded.begin()+start,recorded.end());
     gpu_world=true;start=recorded.size();draw(source.data(),source.size(),2);barrier();
     assert(recorded.size()-start==expected.size());
     for(size_t i=0;i<expected.size();i++)assert(!memcmp(&expected[i],&recorded[start+i],sizeof(RecordedVertex)));
   };
   compare();unsigned converted=mesh_cache.converted;compare();
   assert(mesh_cache.hits>0 && mesh_cache.converted==converted && mesh_cache.reused>=6);
   // An in-flight buffer must not be rewritten for another instance's fog.
   auto saved=source;for(unsigned i=0;i<6;i++)source[i*stride+fog+3]=.87f;
   compare();assert(mesh_cache.blocked>0);
   mesh_cache.begin_frame();compare();assert(mesh_cache.hits>0 && !mesh_cache.converted);
   source=saved;source[0]+=.125f;mesh_cache.begin_frame();compare();assert(mesh_cache.converted>0);
   source[4]+=.125f;mesh_cache.begin_frame();compare();assert(mesh_cache.converted>0);
   source.back()=.33f;mesh_cache.begin_frame();compare();assert(mesh_cache.converted>0);
   mesh_cache.clear();assert(!mesh_cache.bytes());gpu_world=false;
   puts("PASS persistent GPU meshes: exact actual backend vertices/materials, cross-frame reuse without conversion, dynamic fog, in-flight protection, position/UV/colour mutation and cleanup");
 }
 {
   PicaMeshCache bounded;PicaInputPlan plan={};float fog[3]={};
   std::vector<float> source(768*8);std::vector<Vertex> packed(768);
   PicaMeshCache::Key key={1,0,768,8,~0u,{}};unsigned stored=0;
   for(unsigned frame=0;frame<40;frame++) {
     bounded.begin_frame();
     for(unsigned item=0;item<8;item++) {
       source[0]=float(frame*8+item);uint32_t hash;
       assert(!bounded.find(key,source.data(),hash));
       if(bounded.store(key,hash,source.data(),packed.data(),plan,false,fog))stored++;
       assert(bounded.bytes()<=1024*1024);
     }
   }
   assert(stored>40);bounded.clear();assert(!bounded.bytes());
   puts("PASS GPU mesh memory: 1 MiB bound, four promotions per frame, protected active buffers and repeated eviction/cleanup");
 }
 // Regression: the original RSP LOD fraction changes with camera W even
 // while the source model is unchanged. Uniform LOD must not switch routing.
 {
   mesh_cache.clear();ShaderProgram test={};test.id0=0xd000020d210c818aULL;test.id1=0x17;
   gfx_cc_get_features(test.id0,test.id1,&test.cc);active=&test;
   assert(test.cc.num_inputs==2 && test.cc.opt_alpha && test.cc.opt_fog);
   auto source=make_vertices(test.cc,.4f);unsigned stride=source.size()/6,offset=stride-8,fog=offset-4;
   native_pica_lod_hint(2,2);gpu_world=true;
   for(unsigned frame=0;frame<30;frame++) {
     mesh_cache.begin_frame();
     for(unsigned v=0;v<6;v++) {
       float lod=frame?float(180+(frame*3+v*5)%76)/255.f:1.f;
       for(unsigned c=0;c<4;c++)source[v*stride+offset+4+c]=lod;
       source[v*stride+fog+3]=float((frame+v)%10)/10.f;
     }
     mesh_cache_enabled=false;size_t start=recorded.size();draw(source.data(),source.size(),2);barrier();
     std::vector<RecordedVertex> expected(recorded.begin()+start,recorded.end());
     mesh_cache_enabled=true;start=recorded.size();draw(source.data(),source.size(),2);barrier();
     assert(recorded.size()-start==expected.size());
     for(size_t i=0;i<expected.size();i++)assert(!memcmp(&expected[i],&recorded[start+i],sizeof(RecordedVertex)));
     assert(inputs.scalar==1 && inputs.primary==0 && inputs.constant==-1);
     for(unsigned v=0;v<6;v++) {
       float encoded=recorded[start+v].uv[2][0];
       assert(std::fabs((encoded*256-.5f)/255-source[v*stride+offset+4])<.000001f);
     }
     if(frame)assert(mesh_cache.hits==1 && mesh_cache.reused==6 && mesh_cache.converted==0);
   }
   // Geometry/UV/light changes still invalidate the buffer.
   source[0]+=.25f;mesh_cache.begin_frame();draw(source.data(),source.size(),2);barrier();assert(mesh_cache.converted==6);
   native_pica_lod_hint(0,0);gpu_world=false;mesh_cache.clear();
   puts("PASS camera LOD mesh reuse: 30 changing camera/fog frames, uniform-to-varying ramp routing, exact cached/uncached GPU vertices/materials, zero reconversion after warmup and geometry mutation rejection");
 }
 // CPU renderer: proven uniform material avoids the full input scan and
 // preserves the exact legacy vertex/material output. No GPU world/cache.
 {
   ShaderProgram test={};test.id0=0xd000020d210c818aULL;test.id1=0x17;
   gfx_cc_get_features(test.id0,test.id1,&test.cc);active=&test;gpu_world=false;
   auto source_data=make_vertices(test.cc,.4f);
   size_t first=recorded.size();draw(source_data.data(),source_data.size(),2);barrier();
   std::vector<RecordedVertex> reference(recorded.begin()+first,recorded.end());
   native_pica_input_hint(2,2);first=recorded.size();unsigned prior=proven_input_draws;
   draw(source_data.data(),source_data.size(),2);barrier();
   assert(proven_input_draws==prior+1);
   for(size_t i=0;i<reference.size();i++)assert(!memcmp(&reference[i],&recorded[first+i],sizeof(RecordedVertex)));
   native_pica_input_hint(0,0);
   // LOD semantics can pick a supported ramp directly, without a vertex scan.
   unsigned stride=source_data.size()/6,offset=stride-8;
   for(unsigned v=0;v<6;v++)for(unsigned k=0;k<4;k++)source_data[v*stride+offset+4+k]=(180+v*5)/255.f;
   CCFeatures noisy=test.cc;noisy.opt_noise=true;PicaInputPlan rejected;
   assert(!pica_proven_inputs(noisy,source_data.data(),offset,0,0,2,2,&rejected));
   native_pica_lod_hint(2,2);prior=proven_input_draws;
   draw(source_data.data(),source_data.size(),2);barrier();assert(proven_input_draws==prior+1 && inputs.scalar==1 && inputs.primary==0);
   native_pica_lod_hint(0,0);
   puts("PASS CPU material planning: proven uniform inputs match exact legacy backend output; original camera-LOD route skips vertex scanning with GPU world/cache disabled");
 }
 // Producer identities reuse only complete equal vertices within one draw.
 // Compare actual ordered GPU vertices and state for textured/fog/noise paths,
 // zero-ID clipped corners, changed generations, and split material draws.
 {
   unsigned cases=0;
   for(auto entry: {std::pair<uint64_t,unsigned>{0x1000,0}, {0x10001000,1},
       {0xd000020d210c818aULL,0x17}, {0xd000030d12181218ULL,0x13}, {0x1081000,9}}) {
     ShaderProgram test={};test.id0=entry.first;test.id1=entry.second;
     gfx_cc_get_features(test.id0,test.id1,&test.cc);active=&test;gpu_world=false;
     auto source=make_vertices(test.cc,.4f);size_t stride=source.size()/6;
     for(unsigned i=3;i<6;i++)memcpy(source.data()+i*stride,source.data()+(5-i)*stride,stride*sizeof(float));
     for(unsigned variant=0;variant<3;variant++) {
       uint64_t ids[6]={257,258,259,259,258,257};
       if(variant==1)ids[3]=ids[4]=ids[5]=0; // clipping has no reusable identity
       if(variant==2 && test.cc.num_inputs>=2) {
         // Different constant colours force recursive draw splitting.
         size_t offset=stride-(test.cc.opt_alpha?4:3)*test.cc.num_inputs;
         for(unsigned i=3;i<6;i++)for(unsigned k=0;k<3;k++)source[i*stride+offset+(test.cc.opt_alpha?4:3)+k]=.75f;
         for(unsigned i=3;i<6;i++)ids[i]+=256;
       }
       native_pica_vertex_ids(nullptr,nullptr,0);
       size_t first=recorded.size();draw(source.data(),source.size(),2);barrier();
       std::vector<RecordedVertex> expected(recorded.begin()+first,recorded.end());
       unsigned reused=converted_reused;
       native_pica_vertex_ids(source.data(),ids,6);
       first=recorded.size();draw(source.data(),source.size(),2);barrier();
       assert(recorded.size()-first==expected.size());
       for(size_t i=0;i<expected.size();i++)assert(!memcmp(&expected[i],&recorded[first+i],sizeof(RecordedVertex)));
       if(!variant)assert(converted_reused>=reused+3);
       if(variant==1)assert(converted_reused==reused);
       native_pica_vertex_ids(nullptr,nullptr,0);
       // Poison every logical slot: planners, recursive splits and conversion
       // must read references, never the unused intermediate float slots.
       auto saved=source;std::vector<const float *> refs(6);
       for(unsigned i=0;i<6;i++)refs[i]=saved.data()+i*stride;
       std::fill(source.begin(),source.end(),std::numeric_limits<float>::quiet_NaN());
       native_pica_vertex_refs(source.data(),refs.data(),6);
       first=recorded.size();draw(source.data(),source.size(),2);barrier();
       assert(recorded.size()-first==expected.size());
       for(size_t i=0;i<expected.size();i++)assert(!memcmp(&expected[i],&recorded[first+i],sizeof(RecordedVertex)));
       native_pica_vertex_refs(nullptr,nullptr,0);source=saved;cases++;

     }
   }
   puts("PASS indirect vertex packing: poisoned logical slots preserve exact backend vertices/materials through all reference, fog, noise and split cases");
   printf("PASS vertex conversion reuse: %u actual-backend cases preserve exact ordered vertices/materials, including fog, UVs, noise, clipped corners and recursive splits\n",cases);
 }
 // Indexed model draws submit the identical triangle stream while storing
 // each shared GPU vertex once. Test actual reference views and split routes.
 {
   auto *elements=static_cast<uint16_t *>(calloc(MAX_VERTICES,sizeof(uint16_t)));assert(elements);
   for(auto material: {std::pair<uint64_t,unsigned>{0x1000,0},{0x10001000,1},{0xd000020d210c818aULL,0x17}}) {
     ShaderProgram test={};test.id0=material.first;test.id1=material.second;
     gfx_cc_get_features(test.id0,test.id1,&test.cc);active=&test;gpu_world=true;watch_scope=false;
     auto source=make_vertices(test.cc,.4f);unsigned stride=source.size()/6;
     for(unsigned i=3;i<6;i++)memcpy(source.data()+i*stride,source.data()+(5-i)*stride,stride*sizeof(float));
     uint64_t ids[]={2561,2562,2563,2563,2562,2561};
     std::vector<const float *> refs(6);for(unsigned i=0;i<6;i++)refs[i]=source.data()+i*stride;
     native_pica_vertex_ids(source.data(),ids,6);native_pica_vertex_refs(source.data(),refs.data(),6);
     world_indices=nullptr;size_t first=recorded.size();draw(source.data(),source.size(),2);barrier();
     std::vector<RecordedVertex> expected(recorded.begin()+first,recorded.end());
     world_indices=elements;world_index_count=0;first=recorded.size();size_t before_vertices=vertex_count;
     draw(source.data(),source.size(),2);barrier();
     assert(vertex_count-before_vertices==3 && world_index_count==6);
     assert(recorded.size()-first==expected.size());
     for(size_t i=0;i<expected.size();i++)assert(!memcmp(&expected[i],&recorded[first+i],sizeof(RecordedVertex)));
     native_pica_vertex_ids(nullptr,nullptr,0);native_pica_vertex_refs(nullptr,nullptr,0);
   }
   free(elements);world_indices=nullptr;world_index_count=0;gpu_world=false;
   puts("PASS indexed GPU models: actual indexed draws preserve exact ordered vertex/material output for shaded, alpha, textured/fog and reference-view input, with three GPU vertices for six corners");
 }
 // CPU rooms retain reference array draws even with an indexing hint,
 // shared IDs and an available index buffer. GPU models still use indices.
 {
   auto *elements=static_cast<uint16_t *>(calloc(MAX_VERTICES,sizeof(uint16_t)));assert(elements);
   gpu_world=false;watch_body=false;
   for(auto material:{std::pair<uint64_t,unsigned>{0x1000,0},{0x10001000,1},{0xd000020d210c818aULL,0x17}}) {
     ShaderProgram test={};test.id0=material.first;test.id1=material.second;
     gfx_cc_get_features(test.id0,test.id1,&test.cc);active=&test;
     auto source=make_vertices(test.cc,.4f);unsigned stride=source.size()/6;
     for(unsigned i=3;i<6;i++)memcpy(source.data()+i*stride,source.data()+(5-i)*stride,stride*sizeof(float));
     uint64_t shared[]={33281,33282,33283,33283,33282,33281},clipped[6]={};
     std::vector<const float *> refs(6);for(unsigned i=0;i<6;i++)refs[i]=source.data()+i*stride;
     native_pica_vertex_refs(source.data(),refs.data(),6);
     for(unsigned mode=0;mode<7;mode++) {
       watch_scope=mode==1;frontend_ui=mode==2;ui_rect_scope=mode==3;viewmodel_scope=mode==4;
       auto saved_depth=sharp_depth;if(mode==5){sharp_depth.test=true;sharp_depth.bias=1;}
       native_pica_vertex_ids(source.data(),mode==6?clipped:shared,6);
       native_pica_cpu_world_hint(0);world_indices=elements;world_index_count=0;
       size_t first=recorded.size();draw(source.data(),source.size(),2);barrier();
       std::vector<RecordedVertex> expected(recorded.begin()+first,recorded.end());
       native_pica_cpu_world_hint(1);first=recorded.size();size_t before=vertex_count;
       unsigned saved_indices=indexed_draws;
       draw(source.data(),source.size(),2);barrier();
       assert(vertex_count-before==6 && indexed_draws==saved_indices);
       assert(recorded.size()-first==expected.size());
       for(size_t i=0;i<expected.size();i++)assert(!memcmp(&expected[i],&recorded[first+i],sizeof(RecordedVertex)));
       sharp_depth=saved_depth;
     }
     native_pica_vertex_ids(nullptr,nullptr,0);native_pica_vertex_refs(nullptr,nullptr,0);
   }
   native_pica_cpu_world_hint(0);watch_scope=frontend_ui=ui_rect_scope=viewmodel_scope=false;
   free(elements);world_indices=nullptr;world_index_count=0;
   puts("PASS CPU room array reference: hints/shared IDs/index buffer cannot select indexed CPU draws; complete ordered clip positions, depth, UVs, colours, fog and materials match arrays across UI/watch/viewmodel/decal/clipped cases");
 }
 // Reusable GPU buffers must preserve all output attributes as transforms
 // and fog change, including repeated instances within one in-flight frame.
 {
   auto *elements=static_cast<uint16_t *>(calloc(MAX_VERTICES,sizeof(uint16_t)));assert(elements);
   world_indices=elements;world_index_count=0;
   uint64_t token=100;
   for(auto material:{std::pair<uint64_t,unsigned>{0x1000,0},{0x10001000,1},{0xd000020d210c818aULL,0x17}}) {
     ShaderProgram test={};test.id0=material.first;test.id1=material.second;
     gfx_cc_get_features(test.id0,test.id1,&test.cc);active=&test;
     auto source=make_vertices(test.cc,.4f);unsigned stride=source.size()/6;
     for(unsigned i=3;i<6;i++)memcpy(source.data()+i*stride,source.data()+(i-3)*stride,stride*sizeof(float));
     uint64_t ids[]={5001,5002,5003,5001,5002,5003};native_pica_vertex_ids(source.data(),ids,6);
     std::vector<const float *> refs(6);for(unsigned i=0;i<6;i++)refs[i]=source.data()+i*stride;
     native_pica_vertex_refs(source.data(),refs.data(),6);++token;
     for(unsigned frame=0;frame<3;frame++) {
       retained_meshes.begin_frame();
       for(unsigned instance=0;instance<2;instance++) {
         NativeGpuWorld world={};for(unsigned i=0;i<4;i++)world.rows[i][i]=1;
         world.rows[0][3]=frame*2.f+instance;world.cull=2;native_pica_gpu_world(&world);
         world.rows[1][3]=frame*.5f;native_pica_gpu_world(&world); // nonzero palette selector
         if(test.cc.num_inputs) {
           unsigned channels=test.cc.opt_alpha?4:3;
           unsigned input=stride-channels*test.cc.num_inputs;
           unsigned primary=test.cc.num_inputs==3?2:0;
           for(unsigned i=0;i<6;i++) {
             float *col=source.data()+i*stride+input+primary*channels;
             col[0]=.2f+frame*.08f+(i%3)*.01f;col[1]=.4f+instance*.03f;col[2]=.6f;
             if(test.cc.opt_alpha)col[3]=.8f-frame*.05f;
           }
         }
         if(test.cc.opt_fog) {
           unsigned fog=stride-(test.cc.opt_alpha?4:3)*test.cc.num_inputs-4;
           for(unsigned i=0;i<6;i++)source[i*stride+fog+3]=.1f*(frame+instance);
         }
         native_pica_retained_hint(0);size_t start=recorded.size();draw(source.data(),source.size(),2);barrier();
         std::vector<RecordedVertex> expected(recorded.begin()+start,recorded.end());
         unsigned before_converted=converted_built;start=recorded.size();native_pica_retained_hint(token);
         draw(source.data(),source.size(),2);barrier();native_pica_retained_hint(0);
         assert(recorded.size()-start==expected.size());
         for(unsigned i=0;i<expected.size();i++)assert(!memcmp(&expected[i],&recorded[start+i],sizeof expected[i]));
         if(frame)assert(converted_built==before_converted);
         native_pica_gpu_world(nullptr);
       }
       if(frame)assert(retained_meshes.hits==2 && retained_meshes.reused==6);
     }
     native_pica_vertex_ids(nullptr,nullptr,0);native_pica_vertex_refs(nullptr,nullptr,0);
   }
   {
     ShaderProgram test={};test.id0=0x1000;gfx_cc_get_features(test.id0,0,&test.cc);active=&test;
     auto source=make_vertices(test.cc,.4f);unsigned stride=source.size()/6;
     for(unsigned i=3;i<6;i++)memcpy(source.data()+i*stride,source.data()+(i-3)*stride,stride*sizeof(float));
     uint64_t ids[]={9001,9002,9003,9001,9002,9003};
     std::vector<const float *> refs(6);for(unsigned i=0;i<6;i++)refs[i]=source.data()+i*stride;
     native_pica_vertex_ids(source.data(),ids,6);native_pica_vertex_refs(source.data(),refs.data(),6);
     std::vector<RecordedVertex> expected;
     for(unsigned pass=0;pass<3;pass++) {
       retained_meshes.begin_frame();size_t first=recorded.size();unsigned before=submissions;
       for(unsigned bone=0;bone<8;bone++) {
         NativeGpuWorld world={};for(unsigned i=0;i<4;i++)world.rows[i][i]=1;
         world.rows[0][3]=bone*.2f;world.cull=2;native_pica_gpu_world(&world);
         native_pica_retained_hint(pass?9000+bone:0);draw(source.data(),source.size(),2);
       }
       barrier();native_pica_gpu_world(nullptr);native_pica_retained_hint(0);
       assert(recorded.size()-first==48);
       if(!pass)expected.assign(recorded.begin()+first,recorded.end());
       else for(unsigned i=0;i<expected.size();i++)assert(!memcmp(&expected[i],&recorded[first+i],sizeof expected[i]));
       if(retained_stream_enabled())assert(submissions-before==1);
       if(pass==2)assert(retained_meshes.hits==8);
     }
     native_pica_vertex_ids(nullptr,nullptr,0);native_pica_vertex_refs(nullptr,nullptr,0);
     puts("PASS retained stream batching: eight distinct bone matrices share one ordered draw with exact attributes, cache reuse, palette selectors and immutable frame copies");
   }
   retained_meshes.flush();retained_meshes.clear();assert(!retained_meshes.bytes());
   free(elements);world_indices=nullptr;world_index_count=0;
   puts("PASS retained GPU backend: persistent indexed buffers avoid conversion across frames with exact material/UV/colour/position output, changing fog/matrices, multiple instances, palette selectors and cleanup");
 }
 // Persistent templates preserve camera movement, fog and depth LOD while
 // producer identity changes invalidate UV/colour edits and recycled slots.
 {
   unsigned cases=0;
   for(auto entry: {std::pair<uint64_t,unsigned>{0x1000,0},{0x10001000,1},{0xd000020d210c818aULL,0x17}}) {
     ShaderProgram test={};test.id0=entry.first;test.id1=entry.second;
     gfx_cc_get_features(test.id0,test.id1,&test.cc);active=&test;gpu_world=false;
     for(unsigned mode=0;mode<(test.cc.num_inputs==2?3u:1u);mode++) {
       auto source=make_vertices(test.cc,.4f);size_t stride=source.size()/6;
       size_t offset=stride-(test.cc.opt_alpha?4:3)*test.cc.num_inputs;
       uint64_t ids[6];for(unsigned i=0;i<6;i++)ids[i]=((1000+cases)*1024ull)+i;
       for(unsigned frame=0;frame<4;frame++) {
         static_attributes.begin_frame();
         for(unsigned i=0;i<6;i++) {
           float *p=source.data()+i*stride;p[0]+=.04f;p[1]-=.02f;p[2]+=.01f;
           if(test.cc.opt_fog)p[offset-1]=(.05f*i+.1f*frame);
           if(mode) {
             for(unsigned k=0;k<4;k++)p[offset+4+k]=(.1f+.02f*i+.1f*frame);
             if(mode==2)for(unsigned k=0;k<4;k++)p[offset+k]=.5f;
           }
         }
         native_pica_lod_hint(mode?2:0,mode?2:0);native_pica_input_hint(mode==2?1:0,mode==2?1:0);
         native_pica_attribute_ids(nullptr,nullptr,0);
         size_t first=recorded.size();draw(source.data(),source.size(),2);barrier();
         std::vector<RecordedVertex> expected(recorded.begin()+first,recorded.end());
         native_pica_attribute_ids(source.data(),ids,6);
         first=recorded.size();draw(source.data(),source.size(),2);barrier();
         assert(recorded.size()-first==expected.size());
         for(size_t i=0;i<expected.size();i++)assert(!memcmp(&expected[i],&recorded[first+i],sizeof(RecordedVertex)));
         if(frame)assert(static_attributes.hits==6 && static_attributes.cross_frame==6);
         native_pica_attribute_ids(nullptr,nullptr,0);
       }
       // Same cache slots with new versions and edited static attributes.
       for(auto &id:ids)id+=1024ull*100;
       for(unsigned i=0;i<6;i++)source[i*stride+4]+=.125f;
       static_attributes.begin_frame();native_pica_attribute_ids(source.data(),ids,6);
       draw(source.data(),source.size(),2);barrier();assert(!static_attributes.hits && static_attributes.misses==6);
       native_pica_attribute_ids(nullptr,nullptr,0);native_pica_lod_hint(0,0);native_pica_input_hint(0,0);cases++;
     }
   }
   printf("PASS persistent static attributes: %u camera/fog/LOD sequences match exact backend output across frames; primary/scalar LOD refresh and recycled versions verified\n",cases);
 }
 // Distance-selected levels retain the exact base dimensions, UV convention
 // and pixel bytes; full detail resets sampler clamps for subsequent UI.
 {
   Texture tex={};tex.ready=true;tex.gpu.width=64;tex.gpu.height=64;tex.gpu.size=64*64*4;
   tex.gpu.data=calloc(1,tex.gpu.size);assert(tex.gpu.data);live_sharp_textures++;
   auto *pixels=static_cast<unsigned char *>(tex.gpu.data);for(unsigned i=0;i<tex.gpu.size;i++)pixels[i]=i*17;
   std::vector<unsigned char> base(pixels,pixels+tex.gpu.size);
   native_pica_texture_lod(1);ensure_texture_lods(&tex);apply_sampler(&tex);
   assert(tex.levels==3 && tex.gpu.minLevel==1 && tex.gpu.maxLevel==1);
   assert(tex.gpu.width==64 && tex.gpu.height==64 && !memcmp(tex.gpu.data,base.data(),base.size()));
   native_pica_texture_lod(2);apply_sampler(&tex);assert(tex.gpu.minLevel==2 && tex.gpu.maxLevel==2);
   native_pica_texture_lod(3);apply_sampler(&tex);assert(tex.gpu.minLevel==3 && tex.gpu.maxLevel==3);
   native_pica_texture_lod(0);apply_sampler(&tex);assert(!tex.gpu.minLevel && !tex.gpu.maxLevel);
   release_texture_lod_budget(&tex);C3D_TexDelete(&tex.gpu);for(auto &old:retired)C3D_TexDelete(&old);retired.clear();
 }
 {
   Texture tex={};tex.ready=true;tex.gpu.width=64;tex.gpu.height=64;tex.gpu.size=64*64*4;
   texture_lod_extra_live=texture_lod_budget;native_pica_texture_lod(1);
   ensure_texture_lods(&tex);assert(!tex.levels && !tex.gpu.data);
   texture_lod_extra_live=0;native_pica_texture_lod(0);
 }
 puts("PASS three texture LODs: lazy mip allocation, identical base image/UV dimensions, fixed levels 1/2/3, bounded extra memory and full-detail sampler restoration");
 // Replay the exact material/vertex inputs that stopped fd4cc61e9187.
 ShaderProgram captured={};captured.id0=0xd000020d210c818aULL;captured.id1=0x17;
 captured.colors_logged=true;gfx_cc_get_features(captured.id0,captured.id1,&captured.cc);active=&captured;
 auto data=make_vertices(captured.cc,.4f);
 size_t stride=data.size()/6,offset=stride-8;
 for(unsigned v=0;v<6;v++) {
   float *p=data.data()+v*stride+offset;
   p[0]=p[1]=p[2]=180/255.f;p[3]=v%3==0?1.f:0.f;
   p[4]=p[5]=p[6]=(v%3==0?0:v%3==1?254:120)/255.f;p[7]=150/255.f;
 }
 size_t before=recorded.size();draw(data.data(),data.size(),2);barrier();
 assert(recorded.size()==before+6);
 assert(inputs.primary==0 && inputs.scalar==1 && inputs.constant==1 && inputs.scalar_alpha==3);
 auto rgb=source(SHADER_INPUT_2,0),a=source(SHADER_INPUT_2,1);
 assert(rgb.src==GPU_TEXTURE2 && a.src==GPU_CONSTANT && a.constant==2);
 bool alpha150=false;
 for(const auto &env:driver.env)if((env.color>>24)==150)alpha150=true;
 assert(alpha150);
 for(unsigned v=0;v<6;v++)assert(recorded[before+v].colour[3]==(v%3==0?1.f:0.f));
 puts("PASS actual backend captured crate fade: ramp RGB, constant alpha 150, independent primary alpha");
 for(unsigned v=0;v<6;v++) {
   float *p=data.data()+v*stride+offset;
   p[0]=p[1]=p[2]=(v%3==2?179:180)/255.f;p[3]=v%3==0?0.f:1.f;
   p[4]=v%3==0?99/255.f:0.f;p[5]=v%3==0?71/255.f:0.f;p[6]=v%3==0?39/255.f:0.f;p[7]=150/255.f;
 }
 before=recorded.size();draw(data.data(),data.size(),2);barrier();
 assert(recorded.size()==before+6);
 assert(inputs.primary==1 && inputs.scalar==0 && inputs.constant==1 && inputs.primary_alpha==0 && inputs.primary_fixed_alpha==2);
 assert(source(SHADER_INPUT_1,0).src==GPU_TEXTURE2 && source(SHADER_INPUT_1,1).src==GPU_PRIMARY_COLOR);
 assert(source(SHADER_INPUT_2,0).src==GPU_PRIMARY_COLOR && source(SHADER_INPUT_2,1).src==GPU_CONSTANT && source(SHADER_INPUT_2,1).constant==2);
 alpha150=false;for(const auto &env:driver.env)if((env.color>>24)==150)alpha150=true;assert(alpha150);
 for(unsigned v=0;v<6;v++) {
   assert(recorded[before+v].colour[3]==(v%3==0?0.f:1.f));
   assert(!memcmp(recorded[before+v].colour,data.data()+v*stride+offset+4,3*sizeof(float)));
 }
 puts("PASS actual backend coloured crate debris: primary RGB preserved, independent varying/fixed alpha routes");
 // Captured satellite-dish debris: (1 - fixed alpha) * texture alpha.
 captured.id0=0xd30c020d0821818aULL;gfx_cc_get_features(captured.id0,captured.id1,&captured.cc);
 data=make_vertices(captured.cc,.4f);stride=data.size()/6;offset=stride-12;
 for(unsigned v=0;v<6;v++) {
   float *p=data.data()+v*stride+offset;
   p[0]=p[1]=p[2]=192/255.f;p[3]=1.f;
   p[4]=p[5]=p[6]=0.f;p[7]=150/255.f;
   p[8]=p[9]=p[10]=p[11]=0.f;
 }
 before=recorded.size();draw(data.data(),data.size(),2);barrier();assert(recorded.size()==before+6);
 bool inverse=false;
 for(const auto &env:driver.env)if(env.funcAlpha==GPU_MODULATE && (env.opAlpha&15)==GPU_TEVOP_A_ONE_MINUS_SRC_ALPHA)inverse=true;
 assert(inverse);
 puts("PASS captured satellite material: exact inverse-alpha modulation without fatal fallback");
 for(unsigned v=0;v<6;v++) {
   float *p=data.data()+v*stride+offset;
   p[0]=p[1]=p[2]=(v%3==0?195:193)/255.f;p[3]=v%3==2?1.f:0.f;
   p[4]=p[5]=p[6]=v%3==2?0.f:68/255.f;p[7]=150/255.f;
 }
 before=recorded.size();draw(data.data(),data.size(),2);barrier();assert(recorded.size()==before+6);
 assert(inputs.zero_third && source(SHADER_INPUT_1,1).src==GPU_PRIMARY_COLOR && source(SHADER_INPUT_2,1).src==GPU_CONSTANT);
 assert(driver.env[0].funcAlpha==GPU_SUBTRACT && driver.env[1].funcAlpha==GPU_MODULATE);
 for(unsigned v=0;v<6;v++)assert(recorded[before+v].colour[3]==(v%3==2?1.f:0.f));
 puts("PASS captured satellite varying-alpha debris: independent scalar/primary RGB, uniform alpha, zero third input and two-stage alpha");
 // Surface 2 table helicopter: same material, but input 3 is selected as
 // the uniform zero constant rather than eliminated by the two-input fallback.
 for(unsigned mode=0;mode<3;mode++) {
   for(unsigned v=0;v<6;v++) {
     float *p=data.data()+v*stride+offset;
     p[0]=p[1]=p[2]=(v%3==2?181:182)/255.f;
     p[3]=mode==0?0.f:mode==1?1.f:p[0];
     p[4]=p[5]=p[6]=(v%3==0?85:67)/255.f;p[7]=150/255.f;
     p[8]=p[9]=p[10]=p[11]=0.f;
   }
   before=recorded.size();draw(data.data(),data.size(),2);barrier();
   assert(recorded.size()==before+6 && inputs.primary==1 && inputs.constant==2 && inputs.scalar==0 && !inputs.zero_third);
   if(mode!=1)assert(driver.env[0].funcAlpha==GPU_SUBTRACT && driver.env[1].funcAlpha==GPU_MODULATE);
   else assert(driver.env[0].funcAlpha==GPU_MODULATE);
   // Evaluate the actual emitted Citro3D alpha sources/operands/functions.
   // Texture 2 alpha deliberately differs from its R scalar, catching a
   // wrong SRC_ALPHA operand for the ramp without emulating rasterization.
   for(unsigned v=0;v<6;v++)for(unsigned tex=0;tex<256;tex++) {
     const float *p=data.data()+v*stride+offset;
     float previous=0;
     for(unsigned stage=0;stage<2;stage++) {
       const auto &env=driver.env[stage];float args[3]={};
       for(unsigned k=0;k<3;k++) {
         unsigned src=(env.srcAlpha>>(4*k))&15,op=(env.opAlpha>>(4*k))&15;
         float red=0,alpha=0;
         switch(src) {
           case GPU_PRIMARY_COLOR:red=recorded[before+v].colour[0];alpha=recorded[before+v].colour[3];break;
           case GPU_CONSTANT:red=(env.color&255)/255.f;alpha=(env.color>>24)/255.f;break;
           case GPU_TEXTURE0:red=.3f;alpha=tex/255.f;break;
           case GPU_TEXTURE2:red=p[0];alpha=.17f;break;
           case GPU_PREVIOUS:red=alpha=previous;break;
           default:assert(false);
         }
         switch(op) {
           case GPU_TEVOP_A_SRC_ALPHA:args[k]=alpha;break;
           case GPU_TEVOP_A_ONE_MINUS_SRC_ALPHA:args[k]=1-alpha;break;
           case GPU_TEVOP_A_SRC_R:args[k]=red;break;
           case GPU_TEVOP_A_ONE_MINUS_SRC_R:args[k]=1-red;break;
           default:assert(false);
         }
       }
       switch(env.funcAlpha) {
         case GPU_SUBTRACT:previous=args[0]-args[1];break;
         case GPU_MODULATE:previous=args[0]*args[1];break;
         case GPU_ADD:previous=args[0]+args[1];break;
         case GPU_REPLACE:previous=args[0];break;
         default:assert(false);
       }
       previous=std::clamp(previous,0.f,1.f);
     }
     float expected=std::clamp((p[3]-p[7])*(tex/255.f),0.f,1.f);
     assert(std::fabs(previous-expected)<.00001f);
   }
 }
 puts("PASS Surface 2 helicopter debris: zero-third TEV constant, zero/one/ramp alpha routes and 4608 emitted alpha evaluations match the original equation");
 // Statue wreckage can add a nonzero third alpha after the signed difference.
 // Include both signs: clamping A-B before adding C loses the negative term.
 unsigned wreck_evaluations=0;
 for(unsigned fog : {0u,unsigned(SHADER_OPT_FOG)})for(unsigned mode=0;mode<3;mode++)for(unsigned add : {1u,64u,150u,255u,64u}) {
   captured.id1=(captured.id1&~SHADER_OPT_FOG)|fog;
   gfx_cc_get_features(captured.id0,captured.id1,&captured.cc);
   data=make_vertices(captured.cc,.4f);stride=data.size()/6;offset=stride-12;
   for(unsigned v=0;v<6;v++) {
     float *p=data.data()+v*stride+offset;
     p[0]=p[1]=p[2]=(v%3==0?40:v%3==1?150:230)/255.f;
     p[3]=mode==0?p[0]:mode==1?0.f:1.f;
     p[4]=p[5]=p[6]=(v%3==0?85:67)/255.f;p[7]=150/255.f;
     p[8]=p[9]=p[10]=0;p[11]=add/255.f;
   }
   before=recorded.size();draw(data.data(),data.size(),2);barrier();
   assert(recorded.size()==before+6);
   if(mode!=2)assert(driver.buffer_alpha==(1<<1));
   for(unsigned v=0;v<6;v++)for(unsigned tex=0;tex<256;tex++) {
     const float *p=data.data()+v*stride+offset;
     float previous=0,buffer=0;
     for(unsigned stage=0;stage<6;stage++) {
       const auto &env=driver.env[stage];float args[3]={};
       for(unsigned k=0;k<3;k++) {
         unsigned src=(env.srcAlpha>>(4*k))&15,op=(env.opAlpha>>(4*k))&15;
         float red=0,alpha=0;
         switch(src) {
           case GPU_PRIMARY_COLOR:red=recorded[before+v].colour[0];alpha=recorded[before+v].colour[3];break;
           case GPU_CONSTANT:red=(env.color&255)/255.f;alpha=(env.color>>24)/255.f;break;
           case GPU_TEXTURE0:red=.3f;alpha=tex/255.f;break;
           case GPU_TEXTURE2:red=p[inputs.scalar*4];alpha=.17f;break;
           case GPU_PREVIOUS:red=alpha=previous;break;
           case GPU_PREVIOUS_BUFFER:red=alpha=buffer;break;
           default:assert(false);
         }
         switch(op) {
           case GPU_TEVOP_A_SRC_ALPHA:args[k]=alpha;break;
           case GPU_TEVOP_A_ONE_MINUS_SRC_ALPHA:args[k]=1-alpha;break;
           case GPU_TEVOP_A_SRC_R:args[k]=red;break;
           case GPU_TEVOP_A_ONE_MINUS_SRC_R:args[k]=1-red;break;
           default:assert(false);
         }
       }
       switch(env.funcAlpha) {
         case GPU_SUBTRACT:previous=args[0]-args[1];break;
         case GPU_MODULATE:previous=args[0]*args[1];break;
         case GPU_MULTIPLY_ADD:previous=args[0]*args[1]+args[2];break;
         case GPU_ADD:previous=args[0]+args[1];break;
         case GPU_REPLACE:previous=args[0];break;
         default:assert(false);
       }
       previous=std::clamp(previous,0.f,1.f);
       // Only stage 1 writes the buffer, which is consumed at stage 4.
       if(driver.buffer_alpha&(1<<stage))buffer=previous;
     }
     float expected=std::clamp((p[3]-p[7])*(tex/255.f)+p[11],0.f,1.f);
     assert(std::fabs(previous-expected)<.00001f);
     wreck_evaluations++;
   }
 }
 assert(wreck_evaluations==46080);
 puts("PASS Statue wreckage: 46080 signed/additive alpha evaluations, fog on/off, zero/one/ramp A, all texture alpha bytes, live constants and material-cache reuse");
 captured.id1|=SHADER_OPT_FOG;
 // Restore the previous fixture used by watch projection tests.
 captured.id0=0xd000020d210c818aULL;gfx_cc_get_features(captured.id0,captured.id1,&captured.cc);
 data=make_vertices(captured.cc,.4f);stride=data.size()/6;offset=stride-8;
 // Each eye reuses original world vertices with separate world/UI projections;
 // bottom mirroring must see the restored flat matrix.
 for(int samples=1;samples<=2;samples++) {
 top_sample_scale=samples;
 C3D_RenderTarget stereo_left={},stereo_right={};
 target=&stereo_left;right_target=&stereo_right;
 viewport(40,0,320,240);scissor(40,0,320,240);
 stereo_active=true;stereo_slider=1.f;tilt_uniform=0;
 stereo_tilt[0].r[0].x=0.f;stereo_tilt[1].r[0].x=-.01f;stereo_tilt[2].r[0].x=.01f;
 stereo_tilt[3].r[0].x=-.02f;stereo_tilt[4].r[0].x=.02f;
 stereo_tilt[5].r[0].x=-.03f;stereo_tilt[6].r[0].x=.03f;
 size_t u=draw_uniforms.size();before=recorded.size();
 native_pica_frontend_ui(0);draw(data.data(),data.size(),2);
 assert(pending_draw.count==6);
 native_pica_gpu_world(nullptr);native_pica_ui_rect(0);native_pica_set_viewmodel(0);
 assert(pending_draw.count==6 && recorded.size()==before);
 barrier();
 assert(recorded.size()==before+12 && draw_uniforms.size()==u+2);
 assert(draw_uniforms[u]==-.03f && draw_uniforms[u+1]==.03f);
 assert(fabs(draw_eye_offsets[u]+90.f)<.0001f && fabs(draw_eye_offsets[u+1]-90.f)<.0001f);
 assert(draw_world_depth[u]==1.f && draw_world_depth[u+1]==1.f);
 u=draw_uniforms.size();before=recorded.size();
 native_pica_ui_rect(1);draw(data.data(),data.size(),2);barrier();native_pica_ui_rect(0);
 assert(recorded.size()==before+12 && draw_uniforms.size()==u+2);
 assert(draw_uniforms[u]==-.01f && draw_uniforms[u+1]==.01f);
 assert(draw_eye_offsets[u]==0.f && draw_eye_offsets[u+1]==0.f);
 assert(draw_world_depth[u]==0.f && draw_world_depth[u+1]==0.f);
 assert(C3D_FVUnif[0][0].x==0.f && driver.target==(uintptr_t)target);
 for(unsigned v=0;v<6;v++) {
   assert(!memcmp(recorded[before+v].pos,recorded[before+6+v].pos,4*sizeof(float)));
   assert(recorded[before+v].state.target==(uintptr_t)target);
   assert(recorded[before+6+v].state.target==(uintptr_t)right_target);
   assert(!memcmp(recorded[before+v].state.viewport,recorded[before+6+v].state.viewport,sizeof driver.viewport));
 }
 stereo_tilt[9].r[0].x=-.05f;stereo_tilt[10].r[0].x=.05f;
 u=draw_uniforms.size();native_pica_sky(1);native_pica_ui_rect(1);
 draw(data.data(),data.size(),2);barrier();native_pica_ui_rect(0);native_pica_sky(0);
 assert(draw_uniforms[u]==-.05f && draw_uniforms[u+1]==.05f);
 assert(draw_eye_offsets[u]==0.f && draw_eye_offsets[u+1]==0.f && draw_world_depth[u]==1.f);
 // The explicitly tagged reticle selects its own stereo matrices, not the
 // menu/HUD matrix or perspective eye translation. Exiting restores HUD.
 stereo_tilt[7].r[0].x=-.04f;stereo_tilt[8].r[0].x=.04f;
 u=draw_uniforms.size();native_pica_reticle(1);native_pica_ui_rect(1);
 draw(data.data(),data.size(),2);barrier();native_pica_ui_rect(0);native_pica_reticle(0);
 assert(draw_uniforms[u]==-.04f && draw_uniforms[u+1]==.04f);
 assert(draw_eye_offsets[u]==0.f && draw_eye_offsets[u+1]==0.f && reticle_seen);
 u=draw_uniforms.size();before=recorded.size();
 native_pica_set_viewmodel(1);draw(data.data(),data.size(),2);barrier();native_pica_set_viewmodel(0);
 assert(recorded.size()==before+12 && draw_uniforms.size()==u+2);
 assert(draw_uniforms[u]==-.02f && draw_uniforms[u+1]==.02f);
 assert(draw_eye_offsets[u]==-.8f && draw_eye_offsets[u+1]==.8f);
 assert(draw_world_depth[u]==0.f && draw_world_depth[u+1]==0.f);
 assert(C3D_FVUnif[0][0].x==0.f && driver.target==(uintptr_t)target);
 u=draw_uniforms.size();draw(data.data(),data.size(),2);barrier();
 assert(draw_uniforms[u]==-.03f && draw_uniforms[u+1]==.03f);
 assert(fabs(draw_eye_offsets[u]+90.f)<.0001f && fabs(draw_eye_offsets[u+1]-90.f)<.0001f);
 assert(draw_world_depth[u]==1.f && draw_world_depth[u+1]==1.f);
 // Orthographic HUD gauges and the physical watch must not inherit the
 // perspective-world eye translation (W=1 gauges would move off the LCD).
 for(int watch_ui=0;watch_ui<2;watch_ui++) {
   watch_scope=watch_ui!=0;
   native_pica_set_watch_body(1);
   u=draw_uniforms.size();before=recorded.size();
   draw(data.data(),data.size(),2);barrier();
   assert(recorded.size()==before+12);
   assert(draw_uniforms[u]==0.f && draw_uniforms[u+1]==0.f);
   assert(draw_eye_offsets[u]==0.f && draw_eye_offsets[u+1]==0.f);
   assert(draw_world_depth[u]==0.f && draw_world_depth[u+1]==0.f);
   native_pica_set_watch_body(0);
   u=draw_uniforms.size();draw(data.data(),data.size(),2);barrier();
   assert(draw_uniforms[u]==(watch_ui?-.01f:-.03f));
   assert(draw_uniforms[u+1]==(watch_ui?.01f:.03f));
   assert(draw_world_depth[u]==(watch_ui?0.f:1.f));
 }
 watch_scope=false;
 // The model-space GPU path must receive the same full-depth flag and eye
 // separation as CPU-transformed world vertices, in its own uniform bank.
 NativeGpuWorld world={};for(unsigned row=0;row<4;row++)world.rows[row][row]=1.f;
 u=draw_uniforms.size();before=recorded.size();
 native_pica_gpu_world(&world);draw(data.data(),data.size(),2);barrier();native_pica_gpu_world(nullptr);
 assert(recorded.size()==before+12 && draw_eye_offsets.size()==u+2);
 assert(fabs(draw_eye_offsets[u]+90.f)<.0001f && fabs(draw_eye_offsets[u+1]-90.f)<.0001f);
 assert(draw_world_depth[u]==1.f && draw_world_depth[u+1]==1.f);
 assert(C3D_FVUnif[0][world_stereo_uniform].z==0.f);
 stereo_active=false;right_target=nullptr;
 }
 puts("PASS stereo submissions: world/weapon/UI eye projections, redundant scopes preserve queued draws, identical geometry/viewport, flat bottom output");
 // Synthetic completed D24S8 frames exercise the real sampler, not a mock
 // convergence accessor. Cover native, AA and padded filter target layouts.
 for(int samples=1;samples<=2;samples++)for(bool padded:{false,true}){
  top_sample_scale=samples;
  C3D_RenderTarget scene={};scene.frameBuf.width=padded?(samples==2?512:256):240*samples;scene.frameBuf.height=padded?512:400;
  std::vector<uint32_t> depth(size_t(scene.frameBuf.width)*scene.frameBuf.height);
  auto proj=native_stereo::Projection::from(-30010.f/29990.f,-1,-600000.f/29990.f,0);
  float device=-(proj.ratio+proj.b/100.f-1.f)*.5f;
  std::fill(depth.begin(),depth.end(),uint32_t(std::round(device*16777215.f))|0x5a000000u);
  scene.frameBuf.depthBuf=depth.data();target=&scene;
  stereo_active=true;stereo_slider=1;frontend_ui=watch_seen=false;invalidate_stereo_depth();
  for(unsigned i=0;i<120;i++){
   depth_frame={};depth_frame.target=&scene;depth_frame.projection=proj;depth_frame.usable=true;
   depth_frame.width=scene.frameBuf.width;depth_frame.height=scene.frameBuf.height;depth_frame.samples=samples;
   depth_frame.pixels=depth.data();depth_frame.bytes=depth.size()*4;depth_copied=true;
   depth_frame.viewport[0]=40;depth_frame.viewport[1]=0;depth_frame.viewport[2]=320;depth_frame.viewport[3]=240;
   depth_frame.tilt.r[0].y=1;depth_frame.tilt.r[1].x=-1;
   depth_frame.slider=1;depth_frame.convergence=world_convergence;
   reticle_seen=true;reticle_x=200;reticle_y=120;
   update_stereo_depth(1.f/60);
  }
  assert(!depth_frame.usable);
  // Independently write a left/right split in physical tiled storage. A
  // flipped framebuffer row would sample the wrong side at an off-centre aim.
  for(unsigned sy=0;sy<240;sy++)for(unsigned sx=0;sx<400;sx++)for(int sub=0;sub<samples;sub++){
   unsigned mx=(239-sy)*samples+sub,my=scene.frameBuf.height-400+sx;
   float w=sx<200?100.f:200.f;
   float d=-(proj.ratio+proj.b/w-1.f)*.5f;
   depth[native_stereo::tiled_index(mx,my,scene.frameBuf.width)]=uint32_t(std::round(d*16777215.f));
  }
  depth_frame.target=&scene;depth_frame.projection=proj;depth_frame.usable=true;
  depth_frame.width=scene.frameBuf.width;depth_frame.height=scene.frameBuf.height;depth_frame.samples=samples;
  depth_frame.pixels=depth.data();depth_frame.bytes=depth.size()*4;depth_copied=true;
  depth_frame.viewport[0]=40;depth_frame.viewport[1]=0;depth_frame.viewport[2]=320;depth_frame.viewport[3]=240;
  depth_frame.tilt.r[0].y=1;depth_frame.tilt.r[1].x=-1;
  assert(std::fabs(read_stereo_depth(depth_frame,.2f,.3f)-100.f)<.01f);
  assert(std::fabs(read_stereo_depth(depth_frame,.8f,.7f)-200.f)<.01f);
  assert(world_convergence>99 && world_convergence<101);
  assert(std::fabs(reticle_shift)<.0001f);
  // Reconfiguration/menu/watch cannot sample a destroyed/stale surface.
  depth_frame.usable=true;depth_frame.target=nullptr;update_stereo_depth(.016f);
  assert(world_convergence>99 && world_convergence<101);
  depth_frame.usable=true;depth_frame.target=&scene;watch_seen=true;update_stereo_depth(.016f);
  assert(!depth_frame.usable && world_convergence>99 && world_convergence<101 && !aim_depth.have);watch_seen=false;
  target=nullptr;stereo_active=false;
 }
 puts("PASS adaptive stereo backend: completed D24S8 depth, native/AA/padded targets, 4:3 viewport, convergence/reticle depth, stale targets and watch history hold");
 // A queued snapshot stays unchanged when hands/HUD overwrite the scene
 // depth later. Verify producer cache flushing/command-list split precedes
 // the copy and repeated viewmodel entries do not queue another copy.
 for(int samples:{1,2})for(bool padded:{false,true}){
  C3D_RenderTarget scene={};scene.frameBuf.width=padded?(samples==2?512:256):240*samples;scene.frameBuf.height=padded?512:400;
  std::vector<uint32_t> source(size_t(scene.frameBuf.width)*scene.frameBuf.height,0x123456u);scene.frameBuf.depthBuf=source.data();target=&scene;
  depth_copy=static_cast<uint32_t *>(linearAlloc(DEPTH_COPY_CAPACITY));assert(depth_copy);
  depth_frame={};depth_frame.target=&scene;depth_frame.width=scene.frameBuf.width;depth_frame.height=scene.frameBuf.height;depth_frame.usable=true;
  depth_copied=false;stereo_active=true;frontend_ui=false;viewmodel_scope=false;
  unsigned copies=depth_copies;
  native_pica_set_viewmodel(1);assert(depth_copied && depth_copies==copies+1 && depth_frame.pixels==depth_copy);
  std::fill(source.begin(),source.end(),0x999999u);
  assert(depth_copy[1234]==0x123456u);
  native_pica_set_viewmodel(0);native_pica_set_viewmodel(1);assert(depth_copies==copies+1);
  native_pica_set_viewmodel(0);linearFree(depth_copy);depth_copy=nullptr;depth_frame={};depth_copied=false;
  target=nullptr;stereo_active=false;
 }
 puts("PASS GPU depth snapshot ordering: command split before copy, scene frozen before weapons, single copy per frame and bounded native/AA/padded depth bytes");


 // The second LCD keeps native-size UI coordinates and omits watch body.
 // Verify top output, UVs/materials, scissor and buffer restoration.
 for(int samples=1;samples<=2;samples++) {
 top_sample_scale=samples;watch_seen=false;allow_watch=true;watch_clears=0;
 C3D_RenderTarget top_target={},bottom_target={};target=&top_target;watch_target=&bottom_target;
 watch_vertices=static_cast<Vertex *>(calloc(MAX_VERTICES,sizeof(Vertex)));assert(watch_vertices);
 C3D_FrameDrawOn(target);viewport(40,0,320,240);scissor(40,20,320,200);
 for(unsigned i=1;i<textures.size();i++){textures[i]->linear=true;C3D_TexSetFilter(&textures[i]->gpu,GPU_LINEAR,GPU_LINEAR);}
 native_pica_set_watch(1);assert(watch_attached && watch_owner && watch_clears==1);
 before=recorded.size();draw(data.data(),data.size(),2);
 unsigned saved_selected=selected[0];selected[0]=saved_selected==1?2:1;
 barrier();selected[0]=saved_selected;assert(recorded.size()==before+12);
 for(unsigned v=0;v<6;v++) {
   const auto &a=recorded[before+v],&b=recorded[before+6+v];
   assert(a.state.target==(uintptr_t)target && b.state.target==(uintptr_t)watch_target);
   float x=(a.pos[0]/a.pos[3]+1)*160,y=(1-a.pos[1]/a.pos[3])*120;
   assert(b.pos[0]==(native_watch_content_x(x)/160.f-1)*a.pos[3]);
   assert(b.pos[1]==(1-native_watch_content_y(y)/120.f)*a.pos[3]);
   assert(a.pos[2]==b.pos[2] && a.pos[3]==b.pos[3]);assert(!memcmp(a.uv,b.uv,sizeof a.uv));assert(!memcmp(a.colour,b.colour,sizeof a.colour));
   unsigned filter_mask=GPU_TEXTURE_MAG_FILTER(GPU_LINEAR)|GPU_TEXTURE_MIN_FILTER(GPU_LINEAR);
   for(unsigned unit=0;unit<2;unit++)if(active->cc.used_textures[unit]){assert((a.state.texture_params[unit]&filter_mask)==filter_mask);assert((b.state.texture_params[unit]&filter_mask)==filter_mask);}
   assert(a.state.depth==b.state.depth && a.state.alpha==b.state.alpha && a.state.alpha_ref==b.state.alpha_ref);
   assert(!memcmp(a.state.env,b.state.env,sizeof a.state.env));assert(!memcmp(a.state.textures,b.state.textures,sizeof a.state.textures));
   assert(b.state.viewport[0]==0 && b.state.viewport[1]==0 && b.state.viewport[2]==240 && b.state.viewport[3]==320);
 }
 assert(driver.target==(uintptr_t)target && driver_vertices==vertices);
 assert(driver.viewport[0]==0 && driver.viewport[2]==240u*samples && driver.viewport[3]==320);
 assert(driver.scissor[0]==20u*samples && driver.scissor[2]==220u*samples);
 for(unsigned i=1;i<textures.size();i++)assert(textures[i]->gpu.param&GPU_TEXTURE_MAG_FILTER(GPU_LINEAR));
 // Simulate the compressed top glyph positions: the bottom must rebuild
 // its pixel rectangle from authored edges, independently of those positions.
 native_pica_watch_canvas(2.f,.1f);native_pica_watch_rect(64,80,70,90,1);
 before=recorded.size();draw(data.data(),data.size(),2);barrier();assert(recorded.size()==before+12);
 const unsigned corners[6][2]={{0,0},{0,1},{1,0},{0,1},{1,1},{1,0}};
 for(int v=0;v<6;v++) {
   const auto &top=recorded[before+v],&bottom=recorded[before+6+v];
   float x=std::round(native_watch_content_x(corners[v][0]?70:64)),y=std::round(native_watch_content_y(corners[v][1]?90:80));
   assert(std::fabs((bottom.pos[0]/bottom.pos[3]+1)*160-x)<.001f);
   assert(std::fabs((1-bottom.pos[1]/bottom.pos[3])*120-y)<.001f);
   assert(!memcmp(top.uv,bottom.uv,sizeof top.uv));
   assert(!memcmp(top.pos,data.data()+v*stride,sizeof top.pos));
   assert(!memcmp(top.state.env,bottom.state.env,sizeof top.state.env));
   assert(!memcmp(top.state.texture_params,bottom.state.texture_params,sizeof top.state.texture_params));
 }
 native_pica_watch_rect(0,0,0,0,0);native_pica_watch_canvas(1.f,0.f);
 mirror_page=0;native_pica_watch_rect(81,65,90,75,1);
 before=recorded.size();draw(data.data(),data.size(),2);barrier();
 assert(std::fabs((recorded[before+6].pos[0]/recorded[before+6].pos[3]+1)*160-std::round(native_watch_page_content_x(81,0)))<.001f);
 assert(std::fabs((1-recorded[before+6].pos[1]/recorded[before+6].pos[3])*120-std::round(native_watch_page_content_y(65,0)))<.001f);
 assert(native_watch_page_content_y(40,0)==8 && native_watch_page_content_y(212,0)==216);
 assert(native_watch_page_content_x(64,0)==8 && native_watch_page_content_x(256,0)==312);
 native_pica_watch_rect(0,0,0,0,0);
 mirror_page=-1;
 native_pica_set_watch_body(1);before=recorded.size();draw(data.data(),data.size(),2);barrier();assert(recorded.size()==before+6);
 for(unsigned v=0;v<6;v++)assert(recorded[before+v].state.target==(uintptr_t)target);
 native_pica_set_watch_tabs(1);
 for(int tab=0;tab<5;tab++) {
   auto box=data;
   for(int v=0;v<6;v++){box[v*stride]=-.6f+tab*.25f+(v%3==0?.15f:0);box[v*stride+1]=-.7f+(v%3==1?.04f:0);box[v*stride+3]=1;}
   draw(box.data(),box.size(),2);
 }
 native_pica_set_watch_tabs(0);
 for(int i=0;i<5;i++){assert(watch_regions[i][0]>=7+i*62 && watch_regions[i][0]<=8+i*62);assert(watch_regions[i][2]<=65+i*62);assert(watch_regions[i][1]>=225 && watch_regions[i][3]<=233);assert(watch_regions[i][2]<=320 && watch_regions[i][3]<=240);}
 native_pica_set_watch_body(0);native_pica_set_watch(0);before=recorded.size();draw(data.data(),data.size(),2);barrier();assert(recorded.size()==before+6);
 watch_end_output();assert(!watch_attached && !watch_owner);
 allow_watch=false;watch_seen=false;native_pica_set_watch(1);assert(!watch_attached);native_pica_set_watch(0);
 free(watch_vertices);watch_vertices=nullptr;watch_target=nullptr;target=nullptr;
 }
 top_sample_scale=1;
 puts("PASS original watch replay: same top draws, materials/UV/alpha/depth, independent fullscreen UI content mapping, authored pixel glyph rectangles unaffected by compressed top coordinates, excluded watch body/gauges, sharp bottom sampling with restored top filters, flat in-bounds strips, full LCD viewport, scissor/buffer restoration, real page-strip bounds and ownership release");
 // A sloped coplanar marking needs slope offset, not just a fixed D24 unit.
 auto marking=data;
 for(unsigned i=0;i<6;i++) {
   float *p=marking.data()+i*stride;p[0]=i%3==1?1.f:0.f;p[1]=i%3==2?1.f:0.f;
   p[2]=i%3==1?.001f:0.f;p[3]=1;
 }
 viewport(0,0,400,240);scissor(0,0,400,240);depth(true,true,true,false,0xc00);
 before=recorded.size();draw(marking.data(),marking.size(),2);barrier();
 for(unsigned i=0;i<6;i++) {
   const auto &v=recorded[before+i];const float *p=marking.data()+i*stride;
   assert(v.pos[0]==p[0] && v.pos[1]==p[1] && v.pos[2]<p[2] && v.pos[3]==p[3]);
 }
 depth(true,true,true,false,0);before=recorded.size();draw(marking.data(),marking.size(),2);barrier();
 for(unsigned i=0;i<6;i++) {
   const auto &v=recorded[before+i];const float *p=marking.data()+i*stride;
   assert(v.pos[0]==p[0] && v.pos[1]==p[1] && v.pos[3]==p[3] && fabsf(v.pos[2]-p[2])<1.e-7f);
   assert(vertices[vertex_count-6+i].pos[2]==(p[2]-p[3])*.5f);
 }
 puts("PASS actual decal submission: slope-biased reversed depth for road markings; opaque depth uses exact CPU conversion");

 in_frame=false;target=right_target=nullptr;
 top_output_flags=GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8);
 assert(configure_top_samples(1) && live_targets==2 && top_sample_scale==1);
 assert(target->frameBuf.width==240 && right_target->frameBuf.width==240);
 assert(((target->transferFlags>>24)&3)==GX_TRANSFER_SCALE_NO);
 assert(configure_top_samples(2) && live_targets==2 && top_sample_scale==2);
 assert(target->frameBuf.width==480 && target->frameBuf.height==400 && right_target->frameBuf.width==480);
 assert(((target->transferFlags>>24)&3)==GX_TRANSFER_SCALE_X);
 auto *old_left=target,*old_right=right_target;
 fail_create=2;assert(!configure_top_samples(1));
 assert(target==old_left && right_target==old_right && top_sample_scale==2 && live_targets==2);
 assert(outputs[0]==old_left && outputs[1]==old_right && old_left->linked && old_right->linked);
 assert(configure_top_samples(1) && live_targets==2 && top_sample_scale==1);
 C3D_RenderTargetDetachOutput(target);C3D_RenderTargetDetachOutput(right_target);
 C3D_RenderTargetDelete(target);C3D_RenderTargetDelete(right_target);target=right_target=nullptr;assert(!live_targets);
 puts("PASS actual AA target lifecycle: native/2x sizes, hardware resolve flags, both stereo eyes, repeated toggles outside frame, allocation rollback preserving both old outputs");
 // Prototype passes: no framebuffer feedback, both stereo eyes and an
 // allocation failure that restores the normal path outside a frame.
 watch_target=C3D_RenderTargetCreate(240,320,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);
 for(int samples=1;samples<=2;samples++) {
   assert(configure_top_samples(samples));
   update_sharp_targets(samples,1);assert(sharp_enabled && live_targets==5 && live_sharp_textures==2);
   assert(target->frameBuf.width==(samples==2?512:256) && target->frameBuf.height==512);
   assert(sharp_vertices[0].uv[0][0]==0 && sharp_vertices[0].uv[0][1]==0);
   assert(sharp_vertices[2].uv[0][1]==400.f/512 && sharp_vertices[1].uv[0][0]==240.f*samples/sharp_tex[0].width);
   assert(sharp_output[0]->frameBuf.width==240*samples && sharp_output[0]->frameBuf.colorFmt==GPU_RB_RGB8);
   assert(((sharp_output[0]->transferFlags>>8)&7)==GX_TRANSFER_FMT_RGB8);
   in_frame=true;depth(true,true,true,false,0xc00);
   assert(driver.depth_compare==GPU_GEQUAL && driver.depth_bias==4.f/1048576.f);
   depth(true,true,true,false,0);assert(driver.depth_compare==GPU_GREATER && driver.depth_bias==0);
   viewport(40,0,320,240);scissor(40,0,320,240);stereo_active=true;
   depth(true,true,true,false,0);alpha(true,false);
   size_t before_post=recorded.size();sharp_present();assert(recorded.size()==before_post+36);
   for(unsigned eye=0;eye<2;eye++)for(unsigned pass=0;pass<3;pass++) {
     const auto &v=recorded[before_post+eye*18+pass*6];
     assert(v.state.target==(uintptr_t)sharp_output[eye]);
     assert(v.state.textures[0]==(uintptr_t)sharp_tex[eye].data && v.state.textures[1]==(uintptr_t)sharp_tex[eye].data);
     assert(v.state.viewport[2]==240u*samples && v.state.viewport[3]==400);
     assert(v.state.scissor[0]==(pass?1u:0u) && !v.state.depth && !v.state.alpha);
     assert(v.state.blend_eq==(pass==2?GPU_BLEND_REVERSE_SUBTRACT:GPU_BLEND_ADD));
     assert(v.state.blend_dst==(pass?GPU_ONE:GPU_ZERO));
   }
   assert(driver.depth && driver.blend && driver_vertices==vertices && driver.target==(uintptr_t)target);
   assert(driver.viewport[2]==240u*samples && driver.viewport[3]==320);
   in_frame=false;stereo_active=false;
   auto *same=target;update_sharp_targets(samples,3);assert(target==same && sharp_strength==3);
   update_sharp_targets(samples,6);assert(target==same && sharp_strength==6);
   in_frame=true;stereo_active=true;
   before_post=recorded.size();sharp_present();assert(recorded.size()==before_post+36);
   for(unsigned eye=0;eye<2;eye++)for(unsigned pass=0;pass<3;pass++) {
     const auto &state=recorded[before_post+eye*18+pass*6].state;
     assert(state.target==(uintptr_t)sharp_output[eye]);
     assert(state.blend_eq==(pass==1?GPU_BLEND_REVERSE_SUBTRACT:GPU_BLEND_ADD));
   }
   in_frame=false;stereo_active=false;
   update_sharp_targets(samples,0);assert(!sharp_enabled && live_targets==3 && !live_sharp_textures);
   // Changing crouch/zoom must change only the vignette pass, not allocate
   // screen targets or alter the chosen Low/High strength.
   test_vignette=2;test_bloom=test_grain=0;update_sharp_targets(samples,0);
   auto *conditional_target=target;unsigned conditional_targets=live_targets;
   for(int mode=0;mode<4;mode++)for(unsigned pose=0;pose<8;pose++) {
     test_vignette_mode=mode;test_render_state=pose;update_sharp_targets(samples,0);
     assert(target==conditional_target && live_targets==conditional_targets && post_vignette==2);
     in_frame=true;stereo_active=true;size_t conditional_before=recorded.size();sharp_present();
     unsigned expected=6+(native_vignette_active(mode,pose)?POST_VIGNETTE_COUNT:0);
     assert(recorded.size()==conditional_before+2*expected);
     in_frame=false;stereo_active=false;
   }
   test_vignette_mode=0;test_render_state=0;test_vignette=0;update_sharp_targets(samples,0);
   puts("PASS conditional vignette backend: all crouch/zoom/watch/menu states, both eyes/AA sizes, unchanged strength and no pose-triggered target reallocation");
   // Effects work with Image filter Off and share the existing target pair.
   for(int vig=0;vig<=2;vig++)for(int bloom=0;bloom<=2;bloom++)if(vig || bloom) {
     test_vignette=vig;test_bloom=bloom;update_sharp_targets(samples,0);
     assert(sharp_enabled && live_targets==5 && live_sharp_textures==2);
     in_frame=true;stereo_active=true;before_post=recorded.size();sharp_present();
     unsigned per_eye=6+(bloom?24:0)+(vig?POST_VIGNETTE_COUNT:0);
     assert(recorded.size()==before_post+2*per_eye);
     for(unsigned eye=0;eye<2;eye++) {
       unsigned first=before_post+eye*per_eye;
       assert(recorded[first].state.target==(uintptr_t)sharp_output[eye]);
       if(bloom) {
         const auto &b=recorded[first+6];assert(b.state.blend_eq==GPU_BLEND_ADD && b.state.blend_dst==GPU_ONE);
         assert(b.state.textures[1]==(uintptr_t)sharp_tex[eye].data);
         assert(b.state.scissor[0]==0 && b.state.scissor[1]==0);
         for(unsigned j=0;j<24;j++)for(unsigned unit=1;unit<3;unit++) {
           const auto &v=recorded[first+6+j];
           assert(v.uv[unit][0]>=.5f/sharp_tex[eye].width && v.uv[unit][0]<=(240.f*samples-.5f)/sharp_tex[eye].width);
           assert(v.uv[unit][1]>=.5f/512 && v.uv[unit][1]<=399.5f/512);
         }
       }
       if(vig) {
         unsigned start=first+6+(bloom?24:0);
         for(unsigned j=0;j<POST_VIGNETTE_COUNT;j++) {
           const auto &v=recorded[start+j];assert(v.used_uv==0 && !v.state.depth);
           assert(v.pos[0]>=-1 && v.pos[0]<=1 && v.pos[1]>=-1 && v.pos[1]<=1);
           assert(v.colour[3]>=0 && v.colour[3]<=1);
           assert(v.state.blend_src==GPU_SRC_ALPHA && v.state.blend_dst==GPU_ONE_MINUS_SRC_ALPHA);
         }
       }
     }
     in_frame=false;stereo_active=false;
   }
   auto *combined_target=target;update_sharp_targets(samples,6);assert(target==combined_target);
   in_frame=true;stereo_active=true;before_post=recorded.size();sharp_present();
   assert(recorded.size()==before_post+2*(18+24+POST_VIGNETTE_COUNT));
   in_frame=false;stereo_active=false;
   test_grain=2;update_sharp_targets(samples,6);
   in_frame=true;stereo_active=true;before_post=recorded.size();sharp_present();
   unsigned grain_eye=18+24+POST_VIGNETTE_COUNT+12;
   assert(recorded.size()==before_post+2*grain_eye);
   for(unsigned eye=0;eye<2;eye++)for(unsigned pass=0;pass<2;pass++) {
     const auto &g=recorded[before_post+eye*grain_eye+grain_eye-12+pass*6];
     assert(g.state.textures[0]==(uintptr_t)noise_samples[0].data && !g.state.depth);
     assert(g.state.blend_eq==(pass?GPU_BLEND_REVERSE_SUBTRACT:GPU_BLEND_ADD));
   }
   float old_u=sharp_vertices[POST_GRAIN_FIRST].uv[0][0];frame_count++;
   sharp_present();assert(old_u!=sharp_vertices[POST_GRAIN_FIRST].uv[0][0]);
   in_frame=false;stereo_active=false;test_grain=0;
   test_vignette=test_bloom=0;test_grain=1;update_sharp_targets(samples,0);
   in_frame=true;before_post=recorded.size();sharp_present();assert(recorded.size()==before_post+18);
   in_frame=false;test_grain=0;
   test_vignette=test_bloom=0;update_sharp_targets(samples,0);assert(!sharp_enabled && live_targets==3);
   test_vignette=test_bloom=test_grain=2;fail_create=1;update_sharp_targets(samples,0);
   assert(!sharp_enabled && !test_vignette && !test_bloom && !test_grain && live_targets==3);
   fail_create=2;test_sharpness=1;update_sharp_targets(samples,1);
   assert(!sharp_enabled && !test_sharpness && live_targets==3 && !live_sharp_textures);
   sharp_release_targets();assert(live_targets==1);
 }
 C3D_RenderTargetDelete(watch_target);watch_target=nullptr;assert(!live_targets);
 for(int samples:{1,2})for(bool stereo:{false,true}){
  test_sharpness=test_vignette=test_bloom=test_grain=0;test_ghost_contrast=80;test_black_floor=10;
  in_frame=false;update_sharp_targets(samples,0);assert(sharp_enabled);
  in_frame=true;stereo_active=stereo;size_t before=recorded.size();sharp_present();
  assert(recorded.size()==before+(stereo?24:6));
  if(stereo){
   const auto ghost=native_stereo_options::ghost(80,10);
   assert((ghost_blend_colour&255)==unsigned(ghost.a*255+.5f));
   for(unsigned eye=0;eye<2;eye++){
    const auto &state=recorded[before+eye*12+6].state;
    assert(state.blend_src==GPU_ONE && state.blend_dst==GPU_CONSTANT_COLOR);
    assert((state.env[0].color&255)==unsigned(ghost.b*255+.5f));
    assert(!state.depth && !state.alpha);
   }
  }
  in_frame=false;test_ghost_contrast=100;test_black_floor=0;update_sharp_targets(samples,0);
  sharp_release_targets();if(watch_target){C3D_RenderTargetDelete(watch_target);watch_target=nullptr;}assert(!live_targets);
 }
 test_ghost_contrast=80;fail_create=1;update_sharp_targets(1,0);
 assert(test_ghost_contrast==100 && test_black_floor==0 && !sharp_enabled);
 sharp_release_targets();if(watch_target){C3D_RenderTargetDelete(watch_target);watch_target=nullptr;}assert(!live_targets);
 puts("PASS actual GPU ghost reduction: one affine blend per stereo eye, disabled in 2D, native/AA surfaces, preserved alpha/depth and allocation-failure reset");
 { C3D_RenderTarget scene={};target=&scene;stereo_active=true;frontend_ui=watch_seen=false;
   test_stereo_auto=0;test_stereo_convergence=50;test_stereo_crosshair=1;update_stereo_depth(.016f);
   assert(world_convergence==1500 && !aim_depth.have && reticle_shift==0);
   test_stereo_convergence=200;update_stereo_depth(.016f);assert(world_convergence==6000);
   test_stereo_auto=1;test_stereo_convergence=100;test_stereo_crosshair=0;target=nullptr;stereo_active=false;
 }

 // Evaluate the actual configured TEV stages and blend equations against
 // an independent unclamped unsharp reference, including both signs.
 auto fragment=[](const C3D_TexEnv *env,float center,float left,float right) {
   float previous=0,buffer=0;
   for(int stage=0;stage<6;stage++) {
     const auto &e=env[stage];float v[3];
     for(int arg=0;arg<3;arg++) {
       unsigned source=(e.srcRgb>>(arg*4))&15,op=(e.opRgb>>(arg*4))&15;
       v[arg]=source==GPU_TEXTURE0?center:source==GPU_TEXTURE1?left:source==GPU_TEXTURE2?right:source==GPU_PREVIOUS?previous:source==GPU_PREVIOUS_BUFFER?buffer:source==GPU_CONSTANT?(op==GPU_TEVOP_RGB_SRC_ALPHA?(e.color>>24)/255.f:(e.color&255)/255.f):0;
     }
     previous=e.funcRgb==GPU_INTERPOLATE?v[0]*v[2]+v[1]*(1-v[2]):e.funcRgb==GPU_SUBTRACT?v[0]-v[1]:e.funcRgb==GPU_MODULATE?v[0]*v[1]:v[0];
     previous=std::clamp(previous,0.f,1.f);
     if(driver.buffer_rgb&(1u<<stage))buffer=previous;
   }
   return previous;
 };
 for(int level=1;level<=2;level++) {
   post_bloom_stages(level);C3D_TexEnv env[6];memcpy(env,driver.env,sizeof env);
   for(int l=0;l<=255;l+=17)for(int r=0;r<=255;r+=17) {
     float left=l/255.f,right=r/255.f;
     float threshold=(level==1?102:77)/255.f;
     float expected=(std::max(right-threshold,0.f)*(128/255.f)+std::max(left-threshold,0.f)*(127/255.f))*(level==1?64:96)/255.f;
     assert(std::fabs(fragment(env,0,left,right)-expected)<.00001f);
   }
 }
 for(int level=1;level<=2;level++)for(int n=0;n<=239;n++) {
   post_grain_stages(level,0);float positive=fragment(driver.env,n/255.f,0,0);
   post_grain_stages(level,1);float negative=fragment(driver.env,n/255.f,0,0);
   assert(std::fabs((positive-negative)-(n-120)/255.f*(level==1?16:32)/255.f)<.00001f);
 }
 puts("PASS animated film grain: signed neutral noise, two passes, both AA modes/stereo eyes, animated UVs, combined effects and failure cleanup");
 puts("PASS vignette/bloom: independent toggles, both AA sizes/stereo eyes, blend/border/mesh bounds, source separation and actual bloom TEV math");
 for(int level=1;level<=6;level++) {
   C3D_TexEnv passes[3][6];for(int pass=0;pass<3;pass++){sharp_set_stages(pass,level);memcpy(passes[pass],driver.env,sizeof driver.env);}
   for(int c=0;c<=255;c+=17)for(int l=0;l<=255;l+=17)for(int r=0;r<=255;r+=17) {
     float center=c/255.f,left=l/255.f,right=r/255.f;
     float out=fragment(passes[0],center,left,right);
     float sign=level>=4?-1.f:1.f;
     out=std::clamp(out+sign*fragment(passes[1],center,left,right),0.f,1.f);
     out=std::clamp(out-sign*fragment(passes[2],center,left,right),0.f,1.f);
     float difference=center-(left*(128/255.f)+right*(127/255.f));
     float reference;
     if(level>=4)reference=center-((level-3)*64/255.f)*std::copysign(std::max(std::fabs(difference)-8/255.f,0.f),difference);
     else reference=std::clamp(center+(level*64/255.f)*difference,0.f,1.f);
     assert(std::fabs(out-reference)<.00001f);
   }
 }
 if(sharp_vertices){free(sharp_vertices);sharp_vertices=nullptr;}
 puts("PASS sharpness: AA/stereo targets, RGB8 resolve, 3 ordered passes per eye, border protection, state restoration, Off/failure cleanup and 24576 sharpening/smoothing TEV samples");

#endif
 free(vertices);
 for(unsigned i=1;i<textures.size();i++)delete textures[i];
}
