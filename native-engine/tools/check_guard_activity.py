"""Exercise scheduler and actual patched guard/action entry points with engine types."""
import os,re
from pathlib import Path
import build,build_probe
from check_guard_pose import function

def main():
    out=build.BUILD/'guard-activity';out.mkdir(exist_ok=True)
    source=build.source_path(Path('src/game/chraction.c')).read_text()
    for signature in ('bool chrCheckTargetInSight(ChrRecord *self)','s32 chrIsTargetNearlyInSight(ChrRecord *self)'):
        assert function(source,signature).split('{',1)[1].lstrip().startswith('if(!native_guard_activity_sight_allowed(self))return FALSE;')
    action=function(source,'void chrlvActionTick(ChrRecord *self)')
    tick=function(build.source_path(Path('src/game/chr.c')).read_text(),'s32 chrTick(PropRecord *prop)')
    for unit,signature in [('chr.c','Gfx *native_chr_render_impl(PropRecord *prop, Gfx *gdl, s32 withalpha)'),('propobj.c','Gfx *chrobjRenderProp(PropRecord *prop, Gfx *gdl, s32 arg2)')]:
        render=function(build.source_path(Path('src/game')/unit).read_text(),signature)
        assert render.split('{',1)[1].lstrip().startswith('if(!native_guard_activity_drawable(prop))return gdl;')
    names=set(re.findall(r'\b(chrlvTick\w+)\(self\)',action))
    (out/'activity-functions.h').write_text('\n'.join('#define '+n+' native_test_action' for n in sorted(names))+'\n'+action+'\n'+tick+'\n')
    flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
    binary=out/'check'
    build_probe.run(['gcc',*build_probe.engine_args(True),*flags,'-I'+str(out),
        build.TARGET/'tests/guard_activity_host.c','-no-pie','-Wl,--gc-sections','-lm','-o',binary])
    print(build_probe.run([binary],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}).stdout)

if __name__=='__main__':
    build.prepare();main()
