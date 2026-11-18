"""Pass the original aiming sprite scope to the PICA backend."""
def interpreter(text):
    text='#include "native_reticle.h"\nextern "C" void native_pica_reticle(int) __attribute__((weak));\nextern "C" void native_pica_sky(int) __attribute__((weak));\n'+text
    old='static void gfx_sp_extra_geometry_mode(uint32_t clear, uint32_t set) {'
    assert text.count(old)==1
    text=text.replace(old,old+'\n    if((((rsp.extra_geometry_mode & ~clear)|set)^rsp.extra_geometry_mode)&NATIVE_RETICLE_SCOPE) {\n        gfx_flush();if(native_pica_reticle)native_pica_reticle((((rsp.extra_geometry_mode & ~clear)|set)&NATIVE_RETICLE_SCOPE)!=0);\n    }')
    text=text.replace(old,old+'\n    if((((rsp.extra_geometry_mode & ~clear)|set)^rsp.extra_geometry_mode)&NATIVE_SKY_SCOPE) {\n        gfx_flush();if(native_pica_sky)native_pica_sky((((rsp.extra_geometry_mode & ~clear)|set)&NATIVE_SKY_SCOPE)!=0);\n    }')
    # Capture pure perspective matrices from this display list, rather than a
    # mutable game global that may already describe the next queued frame.
    text='#include "native_dynamic_stereo.h"\nextern "C" void native_pica_projection(float,float,float,float) __attribute__((weak));\nstatic bool native_projection_valid;\nstatic float native_projection_coeffs[4];\n'+text
    old='    if (parameters & G_MTX_PROJECTION) {'
    assert text.count(old)==1
    text=text.replace(old,old+'''\n        if(parameters & G_MTX_LOAD){
            float coeffs[]={matrix[2][2],matrix[2][3],matrix[3][2],matrix[3][3]};
            if(native_stereo::Projection::from(coeffs[0],coeffs[1],coeffs[2],coeffs[3]).valid &&
               fabsf(matrix[0][3])+fabsf(matrix[1][3])+fabsf(matrix[3][0])+fabsf(matrix[3][1])<.000001f &&
               (!native_projection_valid || memcmp(coeffs,native_projection_coeffs,sizeof coeffs))){
                gfx_flush();
                if(native_pica_projection)native_pica_projection(coeffs[0],coeffs[1],coeffs[2],coeffs[3]);
                memcpy(native_projection_coeffs,coeffs,sizeof coeffs);native_projection_valid=true;
            }
        }''')
    old='    gfx_rapi->start_frame();'
    assert text.count(old)==1
    text=text.replace(old,old+'\n    native_projection_valid=false;')
    return text

def gun(text):
    text='#include "native_reticle.h"\n'+text
    a='        Gfx *envCommand = sp54;'
    b='        portCrosshairApplyTintCombine(envCommand);'
    assert text.count(a)==1 and text.count(b)==1
    text=text.replace(a,'        gDma0p(sp54++,0x3a,NATIVE_RETICLE_SCOPE,0xffffffu);\n'+a)
    text=text.replace(b,b+'\n        gDma0p(sp54++,0x3a,0,(~NATIVE_RETICLE_SCOPE)&0xffffffu);')
    return text

def level(text):
    text='#include "native_reticle.h"\n'+text
    old='            DL=native_distance_fog_current(&native_fog)?native_distance_fog_backdrop(DL):skyRender(DL);'
    assert text.count(old)==1
    text=text.replace(old,'            gDma0p(DL++,0x3a,NATIVE_SKY_SCOPE,0xffffffu);\n'+old+'\n            gDma0p(DL++,0x3a,0,(~NATIVE_SKY_SCOPE)&0xffffffu);')
    return text
