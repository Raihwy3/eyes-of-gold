"""Cosmetic-only patches; explosion damage and projectile simulation untouched."""
import re
def body_patch(text,signature,patch):
    match=re.search(re.escape(signature)+r'\s*\{',text);assert match,signature
    b=match.end();end=b;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[:b]+patch(text[b:end-1])+text[end-1:]
def prefix(text,signature,code):
    return body_patch(text,signature,lambda body:'\n'+code+'\n'+body)
def sprite(text,signature,query):
    def patch(body):
        assert body.count('return gdl;')>=1
        return '\n    int native_fx_tier='+query+';\n    if(native_fx_tier<0)return gdl;\n    native_effect_scope(gdl++,native_fx_tier,1);\n'+body.replace('return gdl;','native_effect_scope(gdl++,0,0);\n    return gdl;')
    return body_patch(text,signature,patch)
def explosion(text):
    text='#include "native_effects.h"\n'+text
    text=sprite(text,'Gfx *explosionRenderPart(struct ExplosionPart *arg0, Gfx *gdl, struct coord3d *coord)', 'native_effects_tier(&arg0->pos,fabsf(arg0->size)+fabsf(arg0->rot)+64.f)')
    text=sprite(text,'Gfx *explosionSmokeRenderPart(struct Smoke *smoke, struct SmokePart *smoke_part, Gfx *gdl, struct coord3d *arg3)', 'native_effects_tier(&smoke_part->pos,fabsf(smoke_part->size)*2.f+16.f)')
    text=prefix(text,'Gfx *explosionRenderPropExplosion(PropRecord *prop, Gfx *gdl, s32 withalpha)', """    if(!withalpha)return gdl;
    int native_fx_any=0;
    for(int n=0;n<EXPLOSION_PARTS_LEN;n++) {
        struct ExplosionPart *part=&prop->explosion->parts[n];
        if(part->frame>0 && native_effects_tier(&part->pos,fabsf(part->size)+fabsf(part->rot)+64.f)>=0){native_fx_any=1;break;}
    }
    if(!native_fx_any) {
        if(withalpha) {
            int limit=(s32)(g_ExplosionTypes[prop->explosion->explosion_type].flareanimspeed*15.f);
            for(int n=0;n<EXPLOSION_PARTS_LEN;n++)if(limit<prop->explosion->parts[n].frame)prop->explosion->parts[n].frame=0;
        }
        return gdl;
    }""")
    # Keep the original explosion renderer's frame-expiration cleanup and the
    # entire explosion tick/damage path; only omit cosmetic part submissions.
    text=text.replace('gdl = explosionRenderPart(&temp_s5->parts[i], gdl, temp_s6);','if(native_effects_keep(native_effects_tier(&temp_s5->parts[i].pos,fabsf(temp_s5->parts[i].size)+fabsf(temp_s5->parts[i].rot)+64.f),i))gdl = explosionRenderPart(&temp_s5->parts[i], gdl, temp_s6);')
    text=text.replace('gdl = explosionSmokeRenderPart(smoke, &smoke->parts[i], gdl, temp_s5);','if(native_effects_keep(native_effects_tier(&smoke->parts[i].pos,fabsf(smoke->parts[i].size)*2.f+16.f),i))gdl = explosionSmokeRenderPart(smoke, &smoke->parts[i], gdl, temp_s5);')
    # Cosmetic smoke is short-lived; hidden emitters age and drain without
    # spawning/updating particles or doing room-list/bounds work.
    text=prefix(text,'void explosionCreateSmoke(coord3d *pos, StandTile *stan, s16 smoke_type, u8 *rooms, s32 flags)','    if(!native_effects_spawn(pos,512.f))return;')
    text=prefix(text,'s32 explosionSmokeTick(PropRecord *arg0)', '''    if(g_ClockTimer>0 && native_effects_tier(&arg0->pos,512.f)<0) {
        struct Smoke *hidden=arg0->smoke;
        hidden->duration+=g_ClockTimer<15?g_ClockTimer:15;
        for(int n=0;n<SMOKE_PARTS_LEN;n++)hidden->parts[n].size=0;
        if(hidden->duration>=g_SmokeTypes[hidden->smoke_type].duration){hidden->prop=NULL;return TICKOP_FREE;}
        return TICKOP_NONE;
    }''')
    text=prefix(text,'void explosionInitFlyingParticles(coord3d *spawnpos, f32 spawn_rand_scale, f32 spawn_horiz_drift_scale, f32 spawn_vert_drift_scale, f32 spawn_tex_scale)','    if(!native_effects_spawn(spawnpos,fabsf(spawn_rand_scale)*2.f+fabsf(spawn_tex_scale)*2.f))return;')
    text=text.replace('            g_FlyingParticlesBuffer[i].unk00 += (s32) scalar;', '''            if(scalar>0 && native_effects_tier(&g_FlyingParticlesBuffer[i].position,64.f)<0){g_FlyingParticlesBuffer[i].unk00=0;continue;}
            g_FlyingParticlesBuffer[i].unk00 += (s32) scalar;''',1)
    text=text.replace('        if (particles->unk00 > 0)','        if (particles->unk00 > 0 && native_effects_keep(native_effects_tier(&particles->position,64.f),i))',1)
    return text
def glass(text):
    text='#include "native_effects.h"\n'+text
    for signature in ('s_bullet_spark *bullet_spark_create(coord3d *arg0, s32 arg1, f32 arg2, s16 arg3)','s_moving_bullet_spark *bullet_moving_spark_create(coord3d *arg0, coord3d *velocity, s32 arg2, f32 arg3, f32 arg4, s16 arg5)'):
        text=prefix(text,signature,'    if(!native_effects_spawn(arg0,64.f))return NULL;')
    text=prefix(text,'void bullet_spark_render(s_bullet_spark *thing, Gfx *gdlarg, s32 zbufferMode)', '    if(thing->lifetime<=0 || thing->age<0)return;\n    int native_fx_tier=native_effects_tier((coord3d *)&thing->unk10,fabsf(thing->unk24)*2.f+16.f);\n    if(!native_effects_keep(native_fx_tier,(unsigned)((uintptr_t)thing/sizeof *thing)))return;')
    text=text.replace('    gdl = *((Gfx **) gdlarg);','    gdl = *((Gfx **) gdlarg);\n    native_effect_scope(gdl++,native_fx_tier,1);',1)
    text=text.replace('    *((Gfx **) gdlarg) = gdl;','    native_effect_scope(gdl++,0,0);\n    *((Gfx **) gdlarg) = gdl;',1)
    # Existing age/lifetime expiration remains unchanged, including when paused.
    needle='        if (ptr->unk00.lifetime > 0)'
    text=text.replace(needle,'''        if(ptr->unk00.lifetime>0 && g_ClockTimer>0 && native_effects_tier((coord3d *)&ptr->unk00.unk10,64.f)<0){ptr->unk00.lifetime=0;ptr++;continue;}
'''+needle,1)
    return text
def gun(text):
    text='#include "native_effects.h"\n'+text
    text=sprite(text,'Gfx *sub_GAME_7F061E18(Gfx *gdl, BeamRecord *flash, s32 arg2)','flash->unk00<0?-1:native_effects_beam(&flash->pos,&flash->delta,flash->unk1c)')
    text=prefix(text,'void sub_GAME_7F068EC4(CasingRecord *casing, Gfx **gdl)','    if(!native_effects_keep(native_effects_tier(&casing->pos,32.f),(unsigned)((uintptr_t)casing/sizeof *casing)))return;')
    return text
