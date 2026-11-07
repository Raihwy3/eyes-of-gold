"""Execute the actual DSP bridge/backend with a recording libctru boundary."""
import os,tempfile
from pathlib import Path
import build,build_probe

def main():
    out=build.BUILD/'dsp-audio';out.mkdir(exist_ok=True)
    flags=['-std=c11','-O2','-fwrapv','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(build.TARGET/'include')]
    env=dict(os.environ);env['ASAN_OPTIONS']='detect_leaks=0'
    common=build.TARGET/'src/native_dsp_mix.c'
    binary=out/'sidecar-check'
    build_probe.run(['gcc',*flags,common,build.TARGET/'tests/dsp_mix_host.c','-o',binary])
    print(build_probe.run([binary],env=env).stdout)
    build.prepare()
    original=(build.UP/'port/src/mixer.c').read_text()
    patched=build.source_path(Path('port/src/mixer.c')).read_text()
    def env_body(source):
        a=source.index('void aEnvMixerImpl(');b=source.index('\n/* ------------------------------------------------------------------------ */',a)
        return source[a:b]
    (out/'native-env-reference.h').write_text(env_body(original).replace('void aEnvMixerImpl(', 'void original_env('))
    (out/'native-env-dsp.h').write_text(env_body(patched))
    start=patched.index('int native_audio_dsp_fx_pull(');end=patched.index('\n}',start)+2
    (out/'native-fx-capture.h').write_text(patched[start:end])
    binary=out/'envelope-check'
    build_probe.run(['gcc',*flags,'-fno-strict-aliasing','-I'+str(out),common,build.TARGET/'tests/dsp_envelope_host.c','-o',binary])
    print(build_probe.run([binary],env=env).stdout)
    producer=build.source_path(Path('src/audi.c')).read_text()
    start=producer.index('void amHandleFrameMessage(AudioInfo *info, AudioInfo *lastInfo)\n{')
    end=producer.index('\n}',start)+2
    (out/'native-audio-producer.h').write_text(producer[start:end])
    binary=out/'completion-check'
    build_probe.run(['gcc',*flags,'-Wno-pointer-to-int-cast','-I'+str(out),common,build.TARGET/'tests/audio_completion_host.c','-o',binary])
    print(build_probe.run([binary],env=env).stdout)
    obj=out/'sidecars.o';build_probe.run(['gcc',*flags,'-Dcalloc=audio_test_calloc','-c',common,'-o',obj])
    binary=out/'backend-check'
    build_probe.run(['gcc',*flags,'-I'+str(build.TARGET/'tests/audio_mock'),build.TARGET/'src/audio_3ds.c',build.TARGET/'tests/dsp_backend_host.c',obj,'-o',binary])
    with tempfile.TemporaryDirectory() as directory:
        for mode in ('dsp','cpu-setting','linear-fail','stage-fail','ndsp-fail','all-linear-fail','open','open-fx','open-fail','open-fx-fail','open-fx-linear-fail','open-fx-stage-fail','open-stats-fail'):
            print(build_probe.run([binary,mode,directory],env=env).stdout)
        (Path(directory)/'cpu-audio.flag').write_text('')
        print(build_probe.run([binary,'cpu',directory],env=env).stdout)
if __name__=='__main__':main()
