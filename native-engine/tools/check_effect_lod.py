"""Check cosmetic fog/LOD policy and preserve actual gameplay function bodies."""
from pathlib import Path
import os,re
import build,build_probe,effect_lod

def body(text,signature):
    result=[];effect_lod.body_patch(text,signature,lambda s:result.append(s) or s);return result[0]
def main():
    out=build.BUILD/'effect-lod';out.mkdir(exist_ok=True)
    binary=out/'check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',build.TARGET/'tests/effect_lod_host.c','-Wl,--gc-sections','-lm','-o',binary])
    print(build_probe.run([binary],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}).stdout)
    for unit,signatures in {
      'explosion.c':['void explosionInflictDamage(PropRecord *arg0, f32 horiz_range, f32 vert_range)','s32 explosionTick(PropRecord* arg0)'],
      'gunfire.c':['void gunUpdateAndFire(GUNHAND handnum)','void gunTickGameplay(s32 triggerOn)','void update_bullet_casing(CasingRecord* casing)']}.items():
        original=(build.UP/'src/game'/unit).read_text();patched=build.source_path(Path('src/game')/unit).read_text()
        for signature in signatures:assert body(original,signature)==body(patched,signature),signature
    original=(build.UP/'src/game/explosion.c').read_text();patched=build.source_path(Path('src/game/explosion.c')).read_text()
    for signature in ['Gfx *explosionRenderPart(struct ExplosionPart *arg0, Gfx *gdl, struct coord3d *coord)','Gfx *explosionSmokeRenderPart(struct Smoke *smoke, struct SmokePart *smoke_part, Gfx *gdl, struct coord3d *arg3)']:
        b=body(patched,signature)
        assert 'native_fx_tier<0)return gdl;' in b
        # Every original return restores the texture-only scope, including the
        # two near-camera/zero-alpha smoke exits.
        assert b.count('native_effect_scope(gdl++,0,0);')==body(original,signature).count('return gdl;')
    print('PASS cosmetic hooks: explosion damage/ticks, firing/hit path and casing physics unchanged; effect texture scopes restored on every original sprite exit')
if __name__=='__main__':build.prepare();main()
