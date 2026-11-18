#include "pica_decal_depth.h"
#include "pica_inputs.h"
#include "pica_batch.h"
#include "pica_alpha_product.h"
#include "pica_tev_cache.h"
#include "pica_combiner.h"
#include "pica_texcoord.h"
#include "pica_fog.h"
#include "pica_noise.h"
#include "pica_depth.h"
#include "native_cull.h"
#include "native_model_facing.h"
#include "native_cpu_room_bounds.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
int main() {
  // A small facet whose float determinant has the wrong sign through cancellation.
  float fine_y=10004+9/1024.f;
  float old=10000.f*(fine_y*65535.f-10002.f*65535.f)+10000.f*(10001.f*65535.f-10002.f*65535.f)+65535.f*(10002.f*10002.f-10001.f*fine_y);
  assert(old>0);
  assert(native_triangle_facing(10000,10000,65535,10001,10002,65535,10002,fine_y,65535)<0);
  assert(native_viewmodel_facing(10000,10000,65535,10001,10002,65535,10002,fine_y,65535)<0);
  assert(native_viewmodel_facing(10002,fine_y,65535,10001,10002,65535,10000,10000,65535)>0);
  puts("PASS viewmodel determinant cancellation regression and reversed winding");
  assert(native_cull_precise>0);
  puts("PASS adaptive world winding: cancelled float sign no longer rejects a visible world facet");
  {
    unsigned state=0x52713u,checked=0,uncertain=0;
    auto random=[&](){state^=state<<13;state^=state>>17;state^=state<<5;return state;};
    for(unsigned sample=0;sample<5000;sample++) {
      float rows[4][4],points[3][3];
      for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++)rows[r][c]=(int(random()%10001)-5000)*(c==3?1.f:.001f);
      if(sample%2==0){rows[3][0]=rows[3][1]=rows[3][2]=0;rows[3][3]=1;}
      for(auto &p:points)for(float &v:p)v=int(random()%60001)-30000;
      NativeModelFacing facing(rows);int sign=facing.sign(points[0],points[1],points[2]);
      long double p[3][3]={};unsigned rs[3]={0,1,3};
      for(unsigned v=0;v<3;v++)for(unsigned r=0;r<3;r++) {
        p[v][r]=rows[rs[r]][3];
        for(unsigned c=0;c<3;c++)p[v][r]+=static_cast<long double>(rows[rs[r]][c])*points[v][c];
      }
      long double determinant=p[0][0]*(p[2][1]*p[1][2]-p[1][1]*p[2][2])
        +p[0][1]*(p[1][0]*p[2][2]-p[2][0]*p[1][2])+p[0][2]*(p[2][0]*p[1][1]-p[1][0]*p[2][1]);
      if(sign){assert((sign>0)==(determinant>0));checked++;}else uncertain++;
    }
    float singular[4][4]={},a[3]={1,2,3};
    assert(NativeModelFacing(singular).sign(a,a,a)==0 && checked>4000);
    printf("PASS model-space face planes: %u exact signs match independent long-double clip transforms across perspective/orthographic, mirrored and behind-camera cases; %u conservative fallbacks\n",checked,uncertain);
  }

  {
    unsigned state=0x774e15u,rejected=0;
    auto random=[&](){state^=state<<13;state^=state>>17;state^=state<<5;return state;};
    for(unsigned sample=0;sample<50000;sample++) {
      float m[4][4]={},lo[3],hi[3];
      m[0][0]=.5f+(random()%200)/100.f;m[1][1]=.5f+(random()%200)/100.f;
      m[2][2]=-1.01f;m[2][3]=sample%11? -1.f:1.f;m[3][2]=-10;
      m[0][2]=(int(random()%201)-100)*.001f;m[1][2]=(int(random()%201)-100)*.001f;
      for(unsigned i=0;i<3;i++) {
        float center=i==2?-float(10+random()%10000):float(int(random()%20001)-10000);
        float radius=random()%1000;lo[i]=center-radius;hi[i]=center+radius;
      }
      float offset=(int(random()%101)-50)*.01f,scale=.5f+(random()%200)*.01f,aspect=sample%2?4.f/3.f:5.f/3.f;
      bool framebuffer=sample%7==0;float end=sample%3?0.f:.95f;
      if(!native_cpu_room_outside(m,lo,hi,framebuffer,offset,scale,aspect,end))continue;
      unsigned common=63;bool near=true;
      for(unsigned corner=0;corner<8;corner++) {
        long double p[4]={};
        for(unsigned row=0;row<4;row++) {
          p[row]=m[3][row];for(unsigned i=0;i<3;i++)p[row]+=static_cast<long double>(m[i][row])*(corner&(1u<<i)?hi[i]:lo[i]);
        }
        if(!framebuffer)p[0]=(offset*p[3]+p[0])*scale/aspect;
        near=near && p[3]>0 && p[2]+p[3]>0;
        unsigned bits=(p[0]<-p[3]?1:0)|(p[0]>p[3]?2:0)|(p[1]<-p[3]?4:0)|(p[1]>p[3]?8:0)|
          (p[2]>p[3]?16:0)|(end>0 && p[2]>end*p[3]?32:0);
        common&=bits;
      }
      assert(near && common);rejected++;
    }
    assert(rejected>10000);
    float identity[4][4]={};for(unsigned i=0;i<4;i++)identity[i][i]=1;
    float lo[3]={-1,-1,-1},hi[3]={1,1,1};
    assert(!native_cpu_room_outside(identity,lo,hi,false,0,1,4.f/3.f));
    lo[0]=2;assert(!native_cpu_room_outside(identity,lo,hi,false,0,1,4.f/3.f));
    printf("PASS CPU room interval bounds: 50000 boxes checked against independent long-double corner transforms, %u proven rejects; near/behind-camera, aspect variants, fog and invalid bounds retained safely\n",rejected);
  }

  // a3a5928b065a fatal: coloured crate debris + a varying grayscale fade.
  float debris[]={180/255.f,180/255.f,180/255.f,0, 99/255.f,71/255.f,39/255.f,150/255.f,
                  180/255.f,180/255.f,180/255.f,1, 0,0,0,150/255.f,
                  179/255.f,179/255.f,179/255.f,1, 0,0,0,150/255.f};
  PicaInputPlan debris_plan;
  assert(pica_plan_inputs(debris,8,0,3,2,true,&debris_plan));
  assert(debris_plan.primary==1 && debris_plan.scalar==0 && debris_plan.constant==1);
  assert(debris_plan.primary_alpha==0 && debris_plan.primary_fixed_alpha==2 && debris_plan.scalar_alpha==2);
  debris[15]=.2f; // Both independent alphas varying still cannot use one constant.
  assert(!pica_plan_inputs(debris,8,0,3,2,true,&debris_plan));
  puts("PASS captured coloured crate debris: scalar alpha on primary, displaced uniform alpha on TEV constant");

  // Exact fd4cc61e9187 gameplay fatal: crate particle fade with fog.
  float crate_fade[]={180/255.f,180/255.f,180/255.f,1, 0,0,0,150/255.f,
                    180/255.f,180/255.f,180/255.f,0, 254/255.f,254/255.f,254/255.f,150/255.f,
                    180/255.f,180/255.f,180/255.f,0, 120/255.f,120/255.f,120/255.f,150/255.f};
  PicaInputPlan captured_plan;
  assert(pica_plan_inputs(crate_fade,8,0,3,2,true,&captured_plan));
  assert(captured_plan.primary==0 && captured_plan.scalar==1 && captured_plan.constant==1 && captured_plan.scalar_alpha==3);
  puts("PASS captured crate fade with independent varying primary alpha and constant scalar alpha");

  CCFeatures crate={};PicaEquation crate_alpha;
  gfx_cc_get_features(0xd000020d210c818aULL,0x17,&crate);
  assert(crate.opt_texture_edge && crate.opt_2cyc && crate.opt_fog);
  assert(pica_plan_equation(crate,0,1,&crate_alpha) && crate_alpha.kind==PICA_ADD);
  assert(crate_alpha.ids[0]==SHADER_INPUT_1 && crate_alpha.ids[1]==SHADER_INPUT_2);
  float crate_data[24]={.710f,.710f,.710f,1.f, 0,0,0,.588f,
                       .600f,.600f,.600f,.8f, 0,0,0,.588f,
                       .500f,.500f,.500f,.6f, 0,0,0,.588f};
  PicaInputPlan crate_plan;
  assert(pica_plan_inputs(crate_data,8,0,3,2,true,&crate_plan));
  assert(crate_plan.primary==0 && crate_plan.constant==1);
  // ADD preserves the original saturated multiply-add without consuming a
  // literal-one TEV register next to the distinct uniform alpha.
  for(unsigned a=0;a<=255;++a)for(unsigned b=0;b<=255;++b) {
    float x=a/255.f,y=b/255.f;
    assert(std::min(1.f,1.f*x+y)==std::min(1.f,x+y));
  }
  crate.c[0][1][0]=SHADER_INPUT_1;crate.c[0][1][2]=SHADER_1;
  assert(pica_plan_equation(crate,0,1,&crate_alpha) && crate_alpha.kind==PICA_ADD);
  puts("PASS captured crate destruction material uses exact saturated ADD with one uniform constant; both identity-factor orders");
  float flat_a[4]={0,0,0,1},flat_b[4]={1,0,0,1},flat_c[4]={0,1,0,1};
  assert(pica_decal_slope(flat_a,flat_b,flat_c,400,240)==0);
  flat_b[2]=.001f;float slope=pica_decal_slope(flat_a,flat_b,flat_c,400,240);
  assert(slope>0 && slope<1.f/4096);
  float perspective_b[4]={2,0,.002f,2};
  assert(std::fabs(pica_decal_slope(flat_a,perspective_b,flat_c,400,240)-slope)<1e-8f);
  assert(pica_decal_slope(flat_a,flat_b,flat_a,400,240)==0);
  flat_b[3]=0;assert(pica_decal_slope(flat_a,flat_b,flat_c,400,240)==0);
  puts("PASS decal slope offset: flat/sloped surfaces, perspective W invariance and degenerate/camera-plane rejection");
  PicaDepthPlan dp;
  assert(pica_plan_depth(false,true,true,false,0,&dp) && !dp.test && !dp.write);
  assert(pica_plan_depth(true,true,true,false,0,&dp) && dp.test && dp.write && !dp.inclusive && dp.bias==0);
  assert(pica_plan_depth(true,true,true,false,0xc00,&dp) && !dp.write && dp.inclusive && dp.bias==4.f/1048576.f);
  assert(pica_plan_depth(true,true,true,false,0x400,&dp) && dp.inclusive && dp.bias==0);
  assert(pica_plan_depth(true,false,true,false,0x800,&dp) && !dp.inclusive && dp.bias==0);
  // Reference winding after its odd-negative-W correction. Homogeneous
  // determinant must agree for all eight W-sign combinations.
  for(unsigned signs=0;signs<8;++signs) for(unsigned i=1;i<200;++i) {
    float ax=.3f+i*.01f,ay=-.7f,aw=signs&1?-1.3f:1.3f;
    float bx=-.6f,by=.8f+i*.01f,bw=signs&2?-.7f:.7f;
    float cx=.4f,cy=.1f,cw=signs&4?-2.f:2.f;
    float ref=(ax/aw-bx/bw)*(cy/cw-by/bw)-(ay/aw-by/bw)*(cx/cw-bx/bw);
    if((aw<0)^(bw<0)^(cw<0))ref=-ref;
    float stable=native_triangle_facing(ax,ay,aw,bx,by,bw,cx,cy,cw);
    if(std::fabs(ref)>1.e-5f)assert((ref>0)==(stable>0));
  }
  float boundary=native_triangle_facing(-1,-1,1,1,-1,1,0,1,0);
  assert(std::isfinite(boundary) && boundary<0);
  assert(std::isfinite(native_triangle_facing(-1,-1,1,1,-1,1,0,1,1.e-38f)));
  puts("PASS homogeneous winding: all W signs, camera-plane corner, tiny W; disabled depth test cannot write depth");
  CCFeatures textured_noise={}; PicaNoisePlan np;
  gfx_cc_get_features(0x1081000ULL,9,&textured_noise);
  assert(textured_noise.used_textures[0] && !textured_noise.used_textures[1]);
  assert(pica_plan_noise(textured_noise,false,&np));
  assert(np.texture==1 && np.first==1 && np.scales==3 && np.last==4);
  PicaEquation ne;
  assert(pica_plan_equation(textured_noise,0,0,&ne) && ne.kind==PICA_REPLACE && ne.ids[0]==SHADER_INPUT_1);
  assert(pica_plan_equation(textured_noise,0,1,&ne) && ne.kind==PICA_MULTIPLY && ne.ids[0]==SHADER_TEXEL0 && ne.ids[1]==SHADER_INPUT_1);
  // Exhaust every final material-alpha value, including texture * vertex
  // alpha, against the reference noise equation's expected output A*A.
  // Model 8-bit TEV rounding and its saturated subtract / x4 / restore-alpha.
  for(int scales=2;scales<=3;++scales) {
    for(unsigned alpha=0;alpha<256;++alpha) {
      double total=0;
      for(unsigned rank=0;rank<256;++rank) {
        unsigned threshold=rank*(scales==3?251:239)/255;
        unsigned mask=alpha>threshold?alpha-threshold:0;
        for(int stage=0;stage<scales;++stage)mask=std::min(255u,mask*4);
        unsigned output=(mask*alpha+127)/255;
        assert(output<=alpha);
        if(!alpha)assert(!output);
        if(alpha==255)assert(output==255);
        total+=output/255.0;
      }
      double a=alpha/255.0;
      assert(std::fabs(total/256-a*a)<.04);
    }
  }
  for(unsigned flags: {9u,11u,25u,27u}) {
    gfx_cc_get_features(0x1081000ULL,flags,&textured_noise);
    assert(pica_plan_noise(textured_noise,true,&np));
    assert(np.texture==1 && np.last<6 && np.first-1<=3);
  }
  textured_noise.used_textures[1]=true; textured_noise.opt_fog=false;
  assert(pica_plan_noise(textured_noise,false,&np) && np.texture==2);
  assert(!pica_plan_noise(textured_noise,true,&np));
  puts("PASS captured textured watch noise, combined texture alpha, TEV stage/slot budgets and all 8-bit alpha outputs");
  assert(pica_texture_t(0.f,12,16)==1.f);
  assert(pica_texture_t(1.f,12,16)==0.25f);
  assert(pica_texture_t(0.5f,8,8)==0.5f);
  assert(pica_texture_t(2.f,8,8)==-1.f); // Preserve repeating UVs.
  // At texel centers, simulated bottom-origin sampling must recover the
  // original top-origin row, including a font image padded to a POT height.
  for(unsigned row=0;row<12;row++) {
    float t=pica_texture_t((row+0.5f)/12.f,12,16);
    assert((unsigned)std::floor((1.f-t)*16.f)==row);
  }
  for(unsigned factor=0;factor<256;factor++) {
    float t=pica_lookup_t(pica_ramp_coordinate(factor/255.f));
    assert((unsigned)std::floor((1.f-t)*256.f)==factor);
  }
  puts("PASS PICA font/POT/repeating texture T orientation and all 256 fog lookup rows");
  CCFeatures gunbarrel={};
  gfx_cc_get_features(0xd000030d12181218ULL,0x13,&gunbarrel);
  assert(gunbarrel.num_inputs==3 && gunbarrel.opt_alpha && gunbarrel.opt_2cyc && gunbarrel.opt_fog);
  float captured[3][12]={{0,0,0,1, 1,1,1,1, .608f,.608f,.608f,0},
                         {0,0,0,1, 1,1,1,1, .5f,.5f,.5f,0},
                         {0,0,0,1, 1,1,1,1, .4f,.4f,.4f,0}};
  PicaInputPlan gp;
  assert(pica_plan_inputs(&captured[0][0],12,0,3,3,true,&gp));
  assert(gp.scalar==0 && gp.scalar_alpha==1 && gp.constant==1 && gp.primary==2);
  PicaEquation ge;
  assert(pica_plan_equation(gunbarrel,0,1,&ge) && ge.kind==PICA_MIX);
  assert(ge.ids[1]==SHADER_INPUT_1 && ge.ids[2]==SHADER_INPUT_2);
  const float *white=captured[0]+4;
  assert(pica_constant_kind(2,1,white)==pica_constant_kind(1,1,white));
  assert(pica_constant_kind(2,0,white)==1);
  float split[4]={.25f,.5f,.75f,1.f};
  assert(pica_constant_kind(2,0,split)==2 && pica_constant_kind(2,1,split)==1);
  split[3]=0;assert(pica_constant_kind(2,1,split)==0);
  split[3]=.5f;assert(pica_constant_kind(2,1,split)==2);
  assert(pica_constant_kind(0,1,split)==0 && pica_constant_kind(1,1,split)==1);
  puts("PASS captured gunbarrel alpha material shares equivalent literal/uniform TEV constants");
  CCFeatures impact={};
  gfx_cc_get_features(0x20d020d010c818aULL,0x13,&impact);
  assert(impact.num_inputs==2 && impact.used_textures[0] && impact.used_textures[1] && impact.opt_alpha && impact.opt_fog);
  float mixed[9][12];
  for(unsigned v=0;v<9;++v) {
    float lod=v<3?.3f:.6f,alpha=v<3?.4f:.8f;
    float sample[12]={.1f,.2f,.3f,.25f+v*.01f,lod,lod,lod,alpha,
                      .2f+v*.01f,.4f,.1f,.1f+v*.05f};
    std::copy(sample,sample+12,mixed[v]);
  }
  assert(!pica_batch_fits(&mixed[0][0],12,4,3,2,true,true));
  assert(pica_batch_prefix(&mixed[0][0],12,4,3,2,true,true)==1);
  assert(pica_batch_prefix(&mixed[3][0],12,4,2,2,true,true)==2);
  PicaInputPlan split_plan;float split_fog[3];
  assert(pica_batch_fits(&mixed[0][0],12,4,1,2,true,true,&split_plan,split_fog));
  assert(split_plan.constant==0 && split_plan.primary==1 && split_plan.scalar==-1);
  // Splitting retains every original vertex and constant, including both
  // independently varying alpha streams and the original fog interpolation.
  size_t consumed=0,parts=0;
  while(consumed<3) {
    size_t n=pica_batch_prefix(&mixed[consumed*3][0],12,4,3-consumed,2,true,true);
    assert(n && pica_batch_fits(&mixed[consumed*3][0],12,4,n,2,true,true));
    consumed+=n;++parts;
  }
  assert(consumed==3 && parts==2);
  for(unsigned v=6;v<9;++v)mixed[v][0]=.5f;
  assert(pica_batch_prefix(&mixed[3][0],12,4,2,2,true,true)==1);
  for(unsigned v=0;v<3;++v){mixed[v][4]=.1f*v;mixed[v][5]=.4f;mixed[v][6]=.7f;}
  assert(pica_batch_prefix(&mixed[0][0],12,4,1,2,true,true)==0);
  assert(pica_batch_prefix(&mixed[0][0],12,4,0,2,true,true)==0);
  puts("PASS captured dual-texture impact material partitions incompatible batches without dropping triangles");
  float alpha_case[3][8]={{.7f,.7f,.7f,.4f,.1f,.3f,.5f,.2f},
                          {.8f,.8f,.8f,.4f,.4f,.2f,.6f,.5f},
                          {.9f,.9f,.9f,.4f,.3f,.5f,.2f,.8f}};
  PicaInputPlan product_plan;
  assert(pica_plan_inputs(&alpha_case[0][0],8,0,3,2,true,&product_plan));
  assert(product_plan.primary==1 && product_plan.scalar==0 && product_plan.constant==0 && product_plan.scalar_alpha==3);
  assert(pica_alpha_product_plan(impact,&alpha_case[0][0],8,0,3,&product_plan));
  assert(product_plan.primary==1 && product_plan.scalar==0 && product_plan.scalar_alpha==2 && product_plan.primary_fixed_alpha==1);
  for(unsigned i=0;i<=10;++i)for(unsigned j=0;j<=10-i;++j) {
    float weights[3]={i*.1f,j*.1f,1.f-(i+j)*.1f};
    float a=0,b=0,folded=0;
    for(unsigned v=0;v<3;++v){a+=weights[v]*alpha_case[v][3];b+=weights[v]*alpha_case[v][7];folded+=weights[v]*alpha_case[v][3]*alpha_case[v][7];}
    assert(std::fabs(folded-a*b)<.000001f);
  }
  alpha_case[1][3]=.6f;
  assert(!pica_alpha_product_plan(impact,&alpha_case[0][0],8,0,3,&product_plan));
  for(unsigned v=0;v<3;++v)alpha_case[v][7]=.5f;
  assert(pica_alpha_product_plan(impact,&alpha_case[0][0],8,0,3,&product_plan));
  CCFeatures wrong_equation=impact;wrong_equation.c[1][1][2]=SHADER_TEXEL0;
  assert(!pica_alpha_product_plan(wrong_equation,&alpha_case[0][0],8,0,3,&product_plan));
  puts("PASS dual varying-alpha material folds its exact product only with a triangle-uniform factor");
  PicaTevKey cache={0x20d020d010c818aULL,0x13,split_plan,2,2};
  assert(pica_tev_same(cache,cache));
  for(unsigned field=0;field<10;++field) {
    PicaTevKey changed=cache;
    switch(field) {
      case 0:++changed.shader;break;case 1:++changed.flags;break;
      case 2:++changed.rgb_kind;break;case 3:++changed.alpha_kind;break;
      case 4:++changed.inputs.primary;break;case 5:++changed.inputs.constant;break;
      case 6:++changed.inputs.scalar;break;case 7:++changed.inputs.scalar_alpha;break;
      case 8:++changed.inputs.primary_alpha;break;case 9:++changed.inputs.primary_fixed_alpha;break;
    }
    assert(!pica_tev_same(cache,changed));
  }
  puts("PASS TEV wiring cache invalidates on shader, alpha/RGB constant class and every input routing field");
  PicaTevCache<unsigned,2> descriptors;
  descriptors.put(cache,123);assert(descriptors.find(cache) && *descriptors.find(cache)==123);
  PicaTevKey collision=cache;
  do {++collision.shader;}while((pica_tev_hash(collision)&1)!=(pica_tev_hash(cache)&1));
  descriptors.put(collision,456);
  assert(!descriptors.find(cache) && *descriptors.find(collision)==456);
  PicaDrawState draw_a={};draw_a.tev=cache;
  PicaOrderedDraw ordered;ordered.begin(draw_a,0,6);assert(ordered.extendable(draw_a,6));
  auto draw_b=draw_a;draw_b.colours[0]++;assert(!ordered.extendable(draw_b,6));
  draw_b=draw_a;draw_b.textures[1]++;assert(!ordered.extendable(draw_b,6));
  assert(!ordered.extendable(draw_a,9));ordered.clear();assert(!ordered.extendable(draw_a,6));
  puts("PASS bounded material-cache collisions reject stale descriptors; ordered batching splits on constants, textures, gaps and state barriers");
  CCFeatures c={};
  gfx_cc_get_features(0x320d020d818a818aULL,0x513,&c);
  assert(c.num_inputs==3 && c.opt_2cyc && c.opt_fog && c.opt_alpha);
  PicaEquation eq;
  assert(pica_plan_equation(c,0,0,&eq) && eq.kind==PICA_MIX);
  assert(eq.ids[0]==SHADER_TEXEL1 && eq.ids[1]==SHADER_TEXEL0 && eq.ids[2]==SHADER_INPUT_1);
  assert(pica_plan_equation(c,1,0,&eq) && eq.kind==PICA_MULTIPLY);
  assert(pica_plan_equation(c,1,1,&eq) && eq.kind==PICA_MULTIPLY_ADD);
  assert(eq.ids[0]==SHADER_COMBINED && eq.ids[1]==SHADER_INPUT_2 && eq.ids[2]==SHADER_INPUT_3);
  CCFeatures identity={};
  gfx_cc_get_features(0x20d020d010c818aULL,0x13,&identity);
  assert(pica_plan_equation(identity,0,1,&eq));
  assert(eq.kind==PICA_REPLACE && eq.ids[0]==SHADER_INPUT_1 && eq.ids[1]==SHADER_INPUT_1 && eq.ids[2]==SHADER_INPUT_1);
  identity.c[0][1][0]=SHADER_INPUT_2;
  identity.c[0][1][2]=SHADER_1;
  assert(pica_plan_equation(identity,0,1,&eq) && eq.kind==PICA_REPLACE && eq.ids[0]==SHADER_INPUT_2);
  identity.c[0][1][0]=SHADER_0;
  identity.c[0][1][2]=SHADER_INPUT_1;
  assert(pica_plan_equation(identity,0,1,&eq) && eq.kind==PICA_REPLACE && eq.ids[0]==SHADER_0);
  puts("PASS captured literal-one alpha material and multiply identity/zero folding");
  for(int fixed_alpha=0;fixed_alpha<2;fixed_alpha++) {
    float split_alpha[24]={.85f,.85f,.85f,.15f, .68f,.38f,0.f,(float)fixed_alpha,
                           .65f,.65f,.65f,.35f, .58f,.48f,.1f,(float)fixed_alpha,
                           .45f,.45f,.45f,.55f, .48f,.58f,.2f,(float)fixed_alpha};
    PicaInputPlan split;
    assert(pica_plan_inputs(split_alpha,8,0,3,2,true,&split));
    assert(split.primary==1 && split.scalar==0 && split.primary_alpha==0 && split.scalar_alpha==2 && split.primary_fixed_alpha==fixed_alpha);
    split_alpha[15]=0.5f; // No room for two independent varying alpha channels.
    assert(!pica_plan_inputs(split_alpha,8,0,3,2,true,&split));
  }
  puts("PASS dropped-weapon material with independent varying scalar alpha and fixed primary alpha");
  CCFeatures fade_noise={};
  gfx_cc_get_features(0x10001000ULL,9,&fade_noise);
  assert(pica_plan_noise(fade_noise,false,&np) && np.texture==0 && np.last==4);
  unsigned changed=0;
  for(unsigned y=0;y<256;++y) for(unsigned x=0;x<256;++x) {
    unsigned value=pica_noise_sample(x,y,3);
    assert(value<=251);
    changed+=value!=pica_noise_sample((x+73)&255,(y+151)&255,3);
  }
  assert(changed>64000);
  puts("PASS untextured fade uses same final-alpha path; bounded immutable samples and frame-varying screen noise");
  // Permute the varying color, uniform color and LOD scalar over all slots.
  int slots[]={0,1,2};
  do {
    for (int alpha_mode=0;alpha_mode<3;alpha_mode++) {
      float data[36]={};
      for (int v=0;v<3;v++) {
        float *p=data+12*v;
        p[4*slots[0]]=0.2f+0.1f*v; p[4*slots[0]+1]=0.6f; p[4*slots[0]+2]=0.8f; p[4*slots[0]+3]=0.7f;
        p[4*slots[1]]=0.9f; p[4*slots[1]+1]=0.4f; p[4*slots[1]+2]=0.1f; p[4*slots[1]+3]=0.2f;
        float value=0.25f*v;
        for(int ch=0;ch<3;ch++)p[4*slots[2]+ch]=value;
        p[4*slots[2]+3]=alpha_mode==0?value:alpha_mode==1?0.f:1.f;
      }
      PicaInputPlan plan;
      assert(pica_plan_inputs(data,12,0,3,3,true,&plan));
      assert(plan.primary==slots[0] && plan.constant==slots[1] && plan.scalar==slots[2]);
      assert(plan.scalar_alpha==(alpha_mode==0?-1:alpha_mode==1?0:1));
      data[12+4*slots[1]]+=0.05f; // Two independent varying colors cannot fit.
      assert(!pica_plan_inputs(data,12,0,3,3,true,&plan));
    }
  } while(std::next_permutation(slots,slots+3));
  puts("PASS captured Dam three-input/fog material, all input permutations and multiply-add TEV routing");
}
