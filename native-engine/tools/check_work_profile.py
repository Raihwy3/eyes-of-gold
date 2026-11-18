"""Check timing accounting and mechanical hooks without console timings."""
from pathlib import Path
import build,build_probe,os
from analyze_worker_timing import summarize

def main():
    out=build.BUILD/'work-profile';out.mkdir(exist_ok=True)
    binary=out/'check'
    build_probe.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(build.TARGET/'include'),build.TARGET/'tests/work_profile_host.c','-o',binary])
    print(build_probe.run([binary],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}).stdout)
    boss=build.source_path(Path('src/boss.c')).read_text()
    worker=build.source_path(Path('port/src/libultra.c')).read_text()
    for stage in ('MAIN','WAIT_GFX','WAIT_VI','SCENE','STAGE_LOAD'):assert 'NATIVE_WORK_'+stage in boss
    for stage in ('RENDER_IDLE','RENDER'):assert 'native_work_profile_end(NATIVE_WORK_'+stage+',' in worker
    sample='Worker timing: stage=render-work core=2 windowMs=5000.00 samples=100 avgMs=10.000 peakMs=30.000 spanShare=20.0%'
    rows=summarize(sample+'\n'+sample+'\nWorker timing: truncated')
    assert len(rows)==1 and rows[0]['samples']==200 and rows[0]['average_ms']==10 and rows[0]['wall_span_share_percent']==20 and rows[0]['peak_ms']==30
    assert not summarize('old log without timing rows')
    print('PASS timing hooks and offline summary: main/render/wait/load spans, weighted windows, actual core grouping and truncated-line tolerance')
if __name__=='__main__':build.prepare();main()
