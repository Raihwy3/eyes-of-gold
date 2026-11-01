"""Compare every baked PCM context against the original C decoder math."""
import os,struct,tempfile
from native_fixture import fnv
from pathlib import Path
import build,build_probe,audio_pcm_assets

def main():
    audio_pcm_assets.generate()
    out=build.BUILD/'audio-pcm';out.mkdir(exist_ok=True)
    text=(build.UP/'port/src/mixer.c').read_text()
    a=text.index('        int shift = *in >> 4;',text.index('void aADPCMdecImpl'))
    b=text.index('        nbytes -= ',a)
    body=text[a:b].replace("s16 (*tbl)[8]", "const s16 (*tbl)[8]")
    header='''typedef int16_t s16;typedef int32_t s32;typedef uint8_t u8;
static int16_t mixerClamp16(int32_t x){return x<-32768?-32768:x>32767?32767:x;}
/* Original signed-shift code is compared verbatim; sanitizer covers the new
 * file loader/cache, not inherited legacy left shifts of negative residuals. */
__attribute__((no_sanitize("undefined")))
static void original_frame(const unsigned char *frame,const int16_t *book,int16_t prev2,int16_t prev1,int16_t *result) {
    int16_t history[32]={0};history[14]=prev2;history[15]=prev1;
    s16 *out=history+16;const u8 *in=frame;
    const s16 (*sAdpcmTable)[2][8]=(const void *)book;
'''+body+'    memcpy(result,history+16,32);\n}\n'
    (out/'native-adpcm-reference.h').write_text(header)
    build.prepare()
    patched=build.source_path(Path('port/src/mixer.c')).read_text()
    a=patched.index('void aADPCMdecImpl(');b=patched.index('\n/* ------------------------------------------------------------------------ */',a)
    hook='\n#define A_INIT 1\n#define A_LOOP 2\ntypedef uint32_t u32;\ntypedef int16_t ADPCM_STATE[16];\nstatic ADPCM_STATE *sCurrentState,*sAdpcmLoopState;\nstatic int16_t sAdpcmTable[8][2][8];\nstatic int native_pcm_book;\nstatic uint64_t native_test_dmem[512];\nstatic struct {unsigned in,out,count;} sCtx;\nstatic FILE *sMixerTraceFile;\nstatic int mixerTraceOn(void){return 0;}\n#define MTRACE(...) ((void)0)\n#define DMEM_U8(a) ((uint8_t *)native_test_dmem+(a))\n#define DMEM_S16(a) ((int16_t *)DMEM_U8(a))\nstatic void *osPhysicalToVirtual(u32 address){(void)address;return sCurrentState;}\n__attribute__((no_sanitize("undefined")))\n'+patched[a:b]
    (out/'native-adpcm-hook.h').write_text(hook)
    binary=out/'check'
    build_probe.run(['gcc','-std=c11','-O2','-fwrapv','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(build.TARGET/'include'),'-I'+str(out),build.TARGET/'src/native_audio_pcm.c',build.TARGET/'tests/audio_pcm_host.c','-o',binary])
    env=dict(os.environ);env['ASAN_OPTIONS']='detect_leaks=0'
    print(build_probe.run([binary,build.BUILD],env=env).stdout)
    with tempfile.TemporaryDirectory() as directory:
        path=Path(directory)/'native-audio.pcm';data=(build.BUILD/'native-audio.pcm').read_bytes()
        for n in (0,7,31,32,100,len(data)-1):
            path.write_bytes(data[:n]);build_probe.run([binary,directory,'0'],env=env)
        bad=bytearray(data);bad[-2]^=1;path.write_bytes(bad);build_probe.run([binary,directory,'0'],env=env)
        # Recompute checksum to reach the structural validator, not just CRC.
        bad=bytearray(data);books=struct.unpack_from('<I',bad,12)[0]
        struct.pack_into('<H',bad,32+260*books,65535)
        struct.pack_into('<I',bad,24,fnv(bad[32:]));path.write_bytes(bad);build_probe.run([binary,directory,'0'],env=env)
        path.write_bytes(data);build_probe.run([binary,directory,'1'],env=env)
    print('PASS PCM file validation: truncated/corrupt/invalid-book files reject safely; valid cache mounts once')
if __name__=='__main__':main()
