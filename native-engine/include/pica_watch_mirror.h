extern "C" int native_watch_mirror_page(void) __attribute__((weak));
static int mirror_page=-1;
/* Flat native-resolution watch UI. Original text/materials are retained;
 * the physical watch body and gauges remain on the top screen. */
static void watch_end_output() {
  if(!watch_attached)return;
  C3D_RenderTargetDetachOutput(watch_target);watch_attached=false;
  if(native_bottom_gpu_release)native_bottom_gpu_release();
}
extern "C" void native_pica_set_watch(int enabled) {
  flush_draw();watch_scope=enabled!=0;
  if(watch_scope)mirror_page=native_watch_mirror_page?native_watch_mirror_page():-1;
  if(watch_scope && watch_target && !watch_seen) {
    watch_seen=true;
    if(!watch_attached && native_bottom_gpu_acquire && native_bottom_gpu_acquire()) {
      C3D_RenderTargetSetOutput(watch_target,GFX_BOTTOM,GFX_LEFT,
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565));
      watch_attached=true;
    }
    unsigned r=0,g=24,b=0;native_theme_rgb(native_setting_watch(),&r,&g,&b);
    C3D_RenderTargetClear(watch_target,C3D_CLEAR_ALL,(r<<24)|(g<<16)|(b<<8)|255,0);
  }
}
extern "C" void native_pica_watch_canvas(float scale,float offset) {
  if(scale==watch_canvas_scale && offset==watch_canvas_offset)return;
  flush_draw();watch_canvas_scale=scale;watch_canvas_offset=offset;
}
extern "C" void native_pica_watch_rect(float x0,float y0,float x1,float y1,int enabled) {
  flush_draw();watch_rect_active=enabled!=0;watch_rect_vertices=0;
  watch_rect_xy[0]=x0;watch_rect_xy[1]=y0;watch_rect_xy[2]=x1;watch_rect_xy[3]=y1;
}
extern "C" void native_pica_set_watch_body(int enabled){flush_draw();watch_body=enabled!=0;}
extern "C" void native_pica_set_watch_tabs(int enabled) {
  flush_draw();watch_tabs=enabled!=0;
  if(watch_tabs)watch_tab_triangles=0;
  else if(native_bottom_watch_regions && watch_tab_triangles>=10)native_bottom_watch_regions(watch_tab_bounds);
}
static void mirror_draw(int first,int count) {
  if(!watch_scope || !watch_attached || !watch_vertices || count<=0 || (watch_body && !watch_tabs))return;
  // Each authored glyph/slider rectangle shares two X and two Y edges.
  // Map and round those once, preserving the exact original float operations.
  float rect_x[2]={},rect_y[2]={};
  if(watch_rect_active && !watch_tabs)for(unsigned edge=0;edge<2;edge++) {
    rect_x[edge]=std::round(native_watch_page_content_x(watch_rect_xy[edge?2:0],mirror_page));
    rect_y[edge]=std::round(native_watch_page_content_y(watch_rect_xy[edge?3:1],mirror_page));
  }
  for(int i=first;i<first+count;i++) {
    Vertex &v=watch_vertices[i];v=vertices[i];
    // Rebuild flat text/slider rectangles from their original pixel edges,
    // never from the already compressed top-screen clip coordinates.
    if(!watch_tabs) {
      float x,y;
      if(watch_rect_active) {
        static const unsigned corners[6][2]={{0,0},{0,1},{1,0},{0,1},{1,1},{1,0}};
        unsigned corner=(watch_rect_vertices+i-first)%6;
        x=rect_x[corners[corner][0]];y=rect_y[corners[corner][1]];
      } else {
        x=(v.pos[0]/v.pos[3]*watch_canvas_scale+watch_canvas_offset+1)*160;
        y=(1-v.pos[1]/v.pos[3])*120;
      }
      if(!watch_rect_active){x=native_watch_page_content_x(x,mirror_page);y=native_watch_page_content_y(y,mirror_page);}
      v.pos[0]=(x/160.f-1)*v.pos[3];v.pos[1]=(1-y/120.f)*v.pos[3];
    }
    if(watch_tabs) {
      const unsigned triangle=watch_tab_triangles+(i-first)/3,tab=triangle/2;
      static const unsigned corners[6][2]={{0,0},{1,0},{0,1},{1,0},{1,1},{0,1}};
      unsigned corner=(triangle%2)*3+(i-first)%3;
      float x=8.f+tab*62.f+corners[corner][0]*56.f,y=226.f+corners[corner][1]*6.f;
      v.pos[0]=x/160.f-1;v.pos[1]=1-y/120.f;v.pos[2]=-.5f;v.pos[3]=1;
    }
  }
  if(watch_rect_active)watch_rect_vertices+=count;
  if(watch_tabs)for(int i=first;i<first+count;i+=3) {
    unsigned tab=watch_tab_triangles/2;
    if(tab<5) {
      if(!(watch_tab_triangles%2)){watch_tab_bounds[tab][0]=watch_tab_bounds[tab][1]=10000;watch_tab_bounds[tab][2]=watch_tab_bounds[tab][3]=-10000;}
      for(int k=0;k<3 && i+k<first+count;k++) {
        const Vertex &v=watch_vertices[i+k];if(v.pos[3]<=0)continue;
        int x=(int)((v.pos[0]/v.pos[3]+1)*160),y=(int)((1-v.pos[1]/v.pos[3])*120);
        watch_tab_bounds[tab][0]=std::min(watch_tab_bounds[tab][0],x);watch_tab_bounds[tab][1]=std::min(watch_tab_bounds[tab][1],y);
        watch_tab_bounds[tab][2]=std::max(watch_tab_bounds[tab][2],x);watch_tab_bounds[tab][3]=std::max(watch_tab_bounds[tab][3],y);
      }
    }
    watch_tab_triangles++;
  }
  C3D_FrameDrawOn(watch_target);C3D_SetViewport(0,0,240,320);
  // Reproject the original clipping rectangle, including inventory scrolling.
  const float vw=std::max(1,last_viewport[2]),vh=std::max(1,last_viewport[3]);
  int x0=(int)std::floor(native_watch_page_content_x((((last_scissor[0]-last_viewport[0])*2/vw-1)*watch_canvas_scale+watch_canvas_offset+1)*160,mirror_page));
  int x1=(int)std::ceil(native_watch_page_content_x((((last_scissor[0]+last_scissor[2]-last_viewport[0])*2/vw-1)*watch_canvas_scale+watch_canvas_offset+1)*160,mirror_page));
  int y0=(int)std::floor(native_watch_page_content_y((last_scissor[1]-last_viewport[1])*240/vh,mirror_page));
  int y1=(int)std::ceil(native_watch_page_content_y((last_scissor[1]+last_scissor[3]-last_viewport[1])*240/vh,mirror_page));
  x0=std::clamp(x0,0,320);x1=std::clamp(x1,0,320);y0=std::clamp(y0,0,240);y1=std::clamp(y1,0,240);
  if(watch_tabs)C3D_SetScissor(GPU_SCISSOR_NORMAL,0,0,240,320);
  else C3D_SetScissor(GPU_SCISSOR_NORMAL,y0,320-x1,y1,320-x0);
  // Replay the actual bound GPU state, not active/selected globals that can
  // already describe the next queued material. Keep the top draw's complete
  // sampler (including glyph linear filtering), colours and alpha unchanged.
  C3D_SetBufInfo(&watch_buffer);C3D_DrawArrays(GPU_TRIANGLES,first,count);
  C3D_FrameDrawOn(target);C3D_SetBufInfo(&top_buffer);
  restore_top_view();
}
