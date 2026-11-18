/* Optional fixed-function unsharp mask or contrast-sensitive smoothing.
 * Modes 4..6 soften signed differences above 8/255 against a local
 * bilinear neighbourhood. This is not FXAA: there is no edge search. Scene targets are texture-backed;
 * the final RGB8 LCD targets deliberately have no depth buffer. At 2x AA,
 * both eyes plus the unchanged bottom target fit in 6 MiB VRAM (~5.69 MiB).
 * The exact original depth format and frozen vertex shader are retained. */
static C3D_Tex sharp_tex[2],sharp_linear[2];
static C3D_RenderTarget *sharp_output[2];
static Vertex *sharp_vertices;
static C3D_BufInfo sharp_buffer;
static bool sharp_enabled;
#include "native_vignette.h"
static int sharp_strength,post_vignette,post_vignette_mode,post_bloom,post_grain;
static constexpr unsigned POST_VIGNETTE_FIRST=30,POST_VIGNETTE_COUNT=16*10*6;
static constexpr unsigned POST_GRAIN_FIRST=POST_VIGNETTE_FIRST+POST_VIGNETTE_COUNT,POST_VERTEX_COUNT=POST_GRAIN_FIRST+6;
static void sharp_release_targets() {
  // Outside a frame only. Delete the linked outputs first; detaching waits
  // for previous draws/transfers before any sampled texture can be freed.
  for(auto &p:sharp_output)if(p){C3D_RenderTargetDelete(p);p=nullptr;}
  if(target){C3D_RenderTargetDelete(target);target=nullptr;}
  if(right_target){C3D_RenderTargetDelete(right_target);right_target=nullptr;}
  for(auto &t:sharp_tex)if(t.data){C3D_TexDelete(&t);t={};}
  sharp_enabled=false;
}
static bool sharp_create_targets(int scale) {
  const int width=scale==2?512:256;
  for(unsigned eye=0;eye<2;eye++) {
    if(!C3D_TexInitVRAM(&sharp_tex[eye],width,512,GPU_RGBA8))return false;
    C3D_TexSetFilter(&sharp_tex[eye],GPU_NEAREST,GPU_NEAREST);
    C3D_TexSetWrap(&sharp_tex[eye],GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
    sharp_linear[eye]=sharp_tex[eye];
    C3D_TexSetFilter(&sharp_linear[eye],GPU_LINEAR,GPU_LINEAR);
    C3D_RenderTarget *scene=C3D_RenderTargetCreateFromTex(&sharp_tex[eye],GPU_TEXFACE_2D,0,GPU_RB_DEPTH24_STENCIL8);
    if(eye==0)target=scene;else right_target=scene;
    if(!scene)return false;
  }
  // Allocate all large scene surfaces before the smaller presentation
  // buffers; interleaving them fragments the two 3 MiB VRAM banks.
  for(unsigned eye=0;eye<2;eye++) {
    sharp_output[eye]=C3D_RenderTargetCreate(240*scale,400,GPU_RB_RGB8,-1);
    if(!sharp_output[eye])return false;
  }
  const unsigned flags=(top_output_flags&~GX_TRANSFER_IN_FORMAT(7))|
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(scale==2?GX_TRANSFER_SCALE_X:GX_TRANSFER_SCALE_NO);
  for(unsigned eye=0;eye<2;eye++)C3D_RenderTargetSetOutput(sharp_output[eye],GFX_TOP,eye?GFX_RIGHT:GFX_LEFT,flags);
  if(!sharp_vertices) {
    sharp_vertices=static_cast<Vertex *>(linearAlloc(POST_VERTEX_COUNT*sizeof(Vertex)));
    if(!sharp_vertices)return false;
    BufInfo_Init(&sharp_buffer);BufInfo_Add(&sharp_buffer,sharp_vertices,sizeof(Vertex),5,0x43210);
  }
  static const unsigned corner[6][2]={{0,0},{1,0},{0,1},{1,0},{1,1},{0,1}};
  for(unsigned i=0;i<6;i++) {
    Vertex &v=sharp_vertices[i];v={};
    float x=corner[i][0],y=corner[i][1];
    v.pos[0]=2*x-1;v.pos[1]=2*y-1;v.pos[2]=-.5f;v.pos[3]=1;
    for(float &c:v.color)c=1;
    // A render-target texture uses framebuffer-normalized coordinates.
    // Physical axes avoid applying the LCD rotation a second time.
    v.uv[0][0]=x*(240.f*scale)/width;v.uv[0][1]=y*400.f/512;
    for(unsigned unit=1;unit<3;unit++) {
      const float delta=unit==1?.5f:-.5f;
      v.uv[unit][0]=v.uv[0][0]+delta/width;
      v.uv[unit][1]=v.uv[0][1]+delta/512;
    }
  }
  // Four symmetric neighbourhood pairs for a small bright-region glow.
  const float offsets[4][2]={{3.5f,0},{0,3.5f},{3.5f,3.5f},{3.5f,-3.5f}};
  for(unsigned tap=0;tap<4;tap++)for(unsigned i=0;i<6;i++) {
    Vertex &v=sharp_vertices[6+tap*6+i];v=sharp_vertices[i];
    for(unsigned unit=1;unit<3;unit++) {
      float sign=unit==1?1.f:-1.f;
      v.uv[unit][0]=std::clamp(v.uv[0][0]+sign*offsets[tap][0]*scale/width,.5f/width,(240.f*scale-.5f)/width);
      v.uv[unit][1]=std::clamp(v.uv[0][1]+sign*offsets[tap][1]/512.f,.5f/512,399.5f/512);
    }
  }
  for(unsigned y=0;y<10;y++)for(unsigned x=0;x<16;x++)for(unsigned i=0;i<6;i++) {
    Vertex &v=sharp_vertices[POST_VIGNETTE_FIRST+(y*16+x)*6+i];v={};
    float px=2.f*(x+corner[i][0])/16-1,py=2.f*(y+corner[i][1])/10-1;
    v.pos[0]=px;v.pos[1]=py;v.pos[2]=-.5f;v.pos[3]=1;
    // The physical LCD axes are rotated, but this symmetric mask is not.
    v.color[3]=std::clamp((px*px+py*py-.25f)/1.4f,0.f,1.f);
  }
  for(unsigned i=0;i<6;i++)sharp_vertices[POST_GRAIN_FIRST+i]=sharp_vertices[i];
  GSPGPU_FlushDataCache(sharp_vertices,POST_VERTEX_COUNT*sizeof(Vertex));
  top_sample_scale=scale;sharp_enabled=true;return true;
}
static void sharp_set_stages(unsigned pass,int strength) {
  for(unsigned i=0;i<6;i++){C3D_TexEnv e;C3D_TexEnvInit(&e);C3D_SetTexEnv(i,&e);}
  C3D_TexEnv *e=C3D_GetTexEnv(0);
  if(!pass){C3D_TexEnvSrc(e,C3D_Both,GPU_TEXTURE0);return;}
  C3D_TexEnvSrc(e,C3D_RGB,GPU_TEXTURE1,GPU_TEXTURE2,GPU_CONSTANT);
  C3D_TexEnvOpRgb(e,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_ALPHA);
  C3D_TexEnvFunc(e,C3D_RGB,GPU_INTERPOLATE);C3D_TexEnvColor(e,0x80000000);
  e=C3D_GetTexEnv(1);
  C3D_TexEnvSrc(e,C3D_RGB,pass==1?GPU_TEXTURE0:GPU_PREVIOUS,pass==1?GPU_PREVIOUS:GPU_TEXTURE0);
  C3D_TexEnvFunc(e,C3D_RGB,GPU_SUBTRACT);
  unsigned gain_stage=2;
  if(strength>=4) {
    // Ignore low-contrast detail. Each RGB channel gets its own threshold;
    // no claim of a luminance edge mask or adaptive sampling direction.
    e=C3D_GetTexEnv(2);
    C3D_TexEnvSrc(e,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT);
    C3D_TexEnvFunc(e,C3D_RGB,GPU_SUBTRACT);
    C3D_TexEnvColor(e,0xff080808);
    gain_stage=3;
  }
  e=C3D_GetTexEnv(gain_stage);
  C3D_TexEnvSrc(e,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT);C3D_TexEnvFunc(e,C3D_RGB,GPU_MODULATE);
  unsigned gain=unsigned(strength>=4?strength-3:strength)*64;
  C3D_TexEnvColor(e,0xff000000u|gain|(gain<<8)|(gain<<16));
}
static void post_bloom_stages(int strength) {
  for(unsigned i=0;i<6;i++){C3D_TexEnv e;C3D_TexEnvInit(&e);C3D_SetTexEnv(i,&e);}
  C3D_TexEnvBufUpdate(C3D_Both,0);C3D_TexEnvBufUpdate(C3D_RGB,1);
  unsigned threshold=strength==1?102:77;
  unsigned colour=0xff000000u|threshold|(threshold<<8)|(threshold<<16);
  // Threshold EACH sample, then average. Averaging before threshold erased
  // isolated highlights next to dark pixels instead of spreading their light.
  for(int i=0;i<2;i++) {
    C3D_TexEnv *e=C3D_GetTexEnv(i);
    C3D_TexEnvSrc(e,C3D_RGB,i?GPU_TEXTURE2:GPU_TEXTURE1,GPU_CONSTANT);
    C3D_TexEnvFunc(e,C3D_RGB,GPU_SUBTRACT);C3D_TexEnvColor(e,colour);
  }
  // Stage 2 passes the second bright sample while the TEV buffer settles.
  C3D_TexEnv *e=C3D_GetTexEnv(3);
  C3D_TexEnvSrc(e,C3D_RGB,GPU_PREVIOUS,GPU_PREVIOUS_BUFFER,GPU_CONSTANT);
  C3D_TexEnvOpRgb(e,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_ALPHA);
  C3D_TexEnvFunc(e,C3D_RGB,GPU_INTERPOLATE);C3D_TexEnvColor(e,0x80000000);
  e=C3D_GetTexEnv(4);C3D_TexEnvSrc(e,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT);
  C3D_TexEnvFunc(e,C3D_RGB,GPU_MODULATE);
  unsigned gain=strength==1?64:96;C3D_TexEnvColor(e,0xff000000u|gain|(gain<<8)|(gain<<16));
}
static void post_grain_stages(int strength,unsigned pass) {
  C3D_TexEnvBufUpdate(C3D_Both,0);
  for(unsigned i=0;i<6;i++){C3D_TexEnv e;C3D_TexEnvInit(&e);C3D_SetTexEnv(i,&e);}
  C3D_TexEnv *e=C3D_GetTexEnv(0);C3D_TexEnvSrc(e,C3D_RGB,GPU_TEXTURE0);
  C3D_TexEnvOpRgb(e,GPU_TEVOP_RGB_SRC_ALPHA);
  e=C3D_GetTexEnv(1);C3D_TexEnvSrc(e,C3D_RGB,pass?GPU_CONSTANT:GPU_PREVIOUS,pass?GPU_PREVIOUS:GPU_CONSTANT);
  C3D_TexEnvFunc(e,C3D_RGB,GPU_SUBTRACT);C3D_TexEnvColor(e,0xff787878);
  e=C3D_GetTexEnv(2);C3D_TexEnvSrc(e,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT);
  C3D_TexEnvFunc(e,C3D_RGB,GPU_MODULATE);
  unsigned gain=strength==1?16:32;C3D_TexEnvColor(e,0xff000000u|gain|(gain<<8)|(gain<<16));
}
static void post_vignette_stages(int strength) {
  C3D_TexEnvBufUpdate(C3D_Both,0);
  for(unsigned i=0;i<6;i++){C3D_TexEnv e;C3D_TexEnvInit(&e);C3D_SetTexEnv(i,&e);}
  C3D_TexEnv *e=C3D_GetTexEnv(0);C3D_TexEnvSrc(e,C3D_Both,GPU_PRIMARY_COLOR);
  e=C3D_GetTexEnv(1);C3D_TexEnvSrc(e,C3D_Alpha,GPU_PREVIOUS,GPU_CONSTANT);
  C3D_TexEnvFunc(e,C3D_Alpha,GPU_MODULATE);
  C3D_TexEnvColor(e,unsigned(strength==1?89:153)<<24);
}
static void sharp_present() {
  if(!sharp_enabled)return;
  bool vignette_visible=native_vignette_active(post_vignette_mode,native_setting_render_state());
  if(post_grain) {
    // FrameBegin has completed the previous frame: this small dynamic quad
    // can now be changed safely. Both stereo eyes receive the same grain.
    float u=float((frame_count*73u)&255u)/256.f,v=float((frame_count*151u)&255u)/256.f;
    for(unsigned i=0;i<6;i++) {
      Vertex &p=sharp_vertices[POST_GRAIN_FIRST+i];p=sharp_vertices[i];
      p.uv[0][0]=(p.pos[0]+1)*120.f/256+u;
      p.uv[0][1]=(p.pos[1]+1)*200.f/256+v;
    }
    GSPGPU_FlushDataCache(sharp_vertices+POST_GRAIN_FIRST,6*sizeof(Vertex));
  }
  C3D_Mtx identity={};identity.r[0].x=identity.r[1].y=identity.r[2].z=identity.r[3].w=1;
  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,tilt_uniform,&identity);
  C3D_SetBufInfo(&sharp_buffer);C3D_TexEnvBufUpdate(C3D_Both,0);
  C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);C3D_AlphaTest(false,GPU_ALWAYS,0);
  for(unsigned eye=0;eye<(stereo_active?2u:1u);eye++) {
    C3D_FrameDrawOn(sharp_output[eye]);
    C3D_SetViewport(0,0,240*top_sample_scale,400);
    C3D_TexBind(0,&sharp_tex[eye]);C3D_TexBind(1,&sharp_linear[eye]);C3D_TexBind(2,&sharp_linear[eye]);
    for(unsigned pass=0;pass<(sharp_strength?3u:1u);pass++) {
      sharp_set_stages(pass,sharp_strength);
      // Copy every border pixel unchanged. Detail passes exclude one pixel
      // so bilinear neighbours cannot read the padded black texture area.
      C3D_SetScissor(GPU_SCISSOR_NORMAL,pass?1:0,pass?1:0,240*top_sample_scale-(pass?1:0),400-(pass?1:0));
      const bool subtract=sharp_strength>=4?pass==1:pass==2;
      C3D_AlphaBlend(subtract?GPU_BLEND_REVERSE_SUBTRACT:GPU_BLEND_ADD,GPU_BLEND_ADD,
        GPU_ONE,pass?GPU_ONE:GPU_ZERO,GPU_ONE,GPU_ZERO);
      C3D_DrawArrays(GPU_TRIANGLES,0,6);
    }
    if(post_bloom) {
      // Source remains the immutable scene, never the presentation buffer.
      // Tap coordinates stay inside valid scene texels, including the border.
      C3D_SetScissor(GPU_SCISSOR_NORMAL,0,0,240*top_sample_scale,400);
      post_bloom_stages(post_bloom);
      C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ONE,GPU_ZERO,GPU_ONE);
      for(unsigned tap=0;tap<4;tap++)C3D_DrawArrays(GPU_TRIANGLES,6+tap*6,6);
    }
    if(post_vignette && vignette_visible) {
      C3D_SetScissor(GPU_SCISSOR_NORMAL,0,0,240*top_sample_scale,400);
      post_vignette_stages(post_vignette);
      C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_ZERO,GPU_ONE);
      C3D_DrawArrays(GPU_TRIANGLES,POST_VIGNETTE_FIRST,POST_VIGNETTE_COUNT);
    }
    if(post_grain) {
      C3D_SetScissor(GPU_SCISSOR_NORMAL,0,0,240*top_sample_scale,400);
      C3D_TexBind(0,&noise_samples[0]);
      for(unsigned pass=0;pass<2;pass++) {
        post_grain_stages(post_grain,pass);
        C3D_AlphaBlend(pass?GPU_BLEND_REVERSE_SUBTRACT:GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ONE,GPU_ZERO,GPU_ONE);
        C3D_DrawArrays(GPU_TRIANGLES,POST_GRAIN_FIRST,6);
      }
    }
    if(stereo_active){
      const auto ghost=native_stereo_options::ghost(native_setting_stereo_ghost_contrast(),native_setting_stereo_black_floor());
      if(ghost.active){
        // The reference affine transform C*A+B needs one constant-colour
        // blend, not another texture sample or a CPU image pass.
        for(unsigned stage=0;stage<6;stage++){C3D_TexEnv env;C3D_TexEnvInit(&env);C3D_SetTexEnv(stage,&env);}
        C3D_TexEnv *env=C3D_GetTexEnv(0);C3D_TexEnvSrc(env,C3D_Both,GPU_CONSTANT);
        const unsigned a=unsigned(ghost.a*255.f+.5f),b=unsigned(ghost.b*255.f+.5f);
        C3D_TexEnvColor(env,0xff000000u|b|(b<<8)|(b<<16));
        C3D_BlendingColor(0xff000000u|a|(a<<8)|(a<<16));
        C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_CONSTANT_COLOR,GPU_ZERO,GPU_ONE);
        C3D_SetScissor(GPU_SCISSOR_NORMAL,0,0,240*top_sample_scale,400);
        C3D_DrawArrays(GPU_TRIANGLES,0,6);
      }
    }
  }
  C3D_TexEnvBufUpdate(C3D_Both,0);
  // Copy + positive detail - negative detail equals clamp(C+k*(C-B)).
  // Splitting the signs avoids losing dark detail to intermediate TEV clamp.
  C3D_FrameDrawOn(target);restore_top_view();C3D_SetBufInfo(&top_buffer);
  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,tilt_uniform,&stereo_tilt[0]);
  C3D_DepthTest(sharp_depth.test,sharp_depth.compare?(sharp_depth.inclusive?GPU_GEQUAL:GPU_GREATER):GPU_ALWAYS,sharp_depth.write?GPU_WRITE_ALL:GPU_WRITE_COLOR);
  C3D_DepthMap(true,sharp_depth_scale,sharp_depth_bias);
  C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,last_blend?GPU_SRC_ALPHA:GPU_ONE,last_blend?GPU_ONE_MINUS_SRC_ALPHA:GPU_ZERO,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA);
  if(active)C3D_AlphaTest(active->cc.opt_texture_edge||active->cc.opt_alpha_threshold,GPU_GREATER,active->cc.opt_texture_edge?127:0);
  for(unsigned unit=0;unit<3;unit++)C3D_TexBind(unit,&scalar_ramp);
  tev_valid=false;
}
