"""Build-only HTTPS/ZIP dependencies; never download dependencies during a build."""
from pathlib import Path
import json,os
import build,build_probe

def prefix():
    candidates=[Path(os.environ.get('EOG_UPDATE_PORTLIBS','/opt/devkitpro/portlibs/3ds')),build.ROOT/'local/update-deps/opt/devkitpro/portlibs/3ds']
    for p in candidates:
        if all((p/'lib'/n).exists() for n in ('libcurl.a','libmbedtls.a','libmbedx509.a','libmbedcrypto.a','libminizip.a','libz.a')):return p
    raise RuntimeError('Updater libraries missing. Install 3ds-curl, 3ds-mbedtls and 3ds-zlib with devkitPro pacman, or set EOG_UPDATE_PORTLIBS to their prefix.')

def objects():
    p=prefix();out=build.BUILD/'updater';out.mkdir(exist_ok=True)
    ca=(build.TARGET/'vendor/updater/cacert.pem').read_text()
    (out/'native_update_ca.h').write_text('static const char native_update_ca[]=\n'+ '\n'.join(json.dumps(line+'\n') for line in ca.splitlines())+';\n')
    flags=['-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-D__3DS__','-DCURL_STATICLIB',
           '-I'+str(build.TARGET/'include'),'-I'+str(build.TARGET/'vendor/updater'),'-I'+str(out),'-I'+str(p/'include'),'-I/opt/devkitpro/libctru/include']
    result=[]
    for name in ('native_update.c','native_update_http.c','native_update_3ds.c'):
        obj=out/(name+'.o');build_probe.run([build.CC,*flags,'-c',build.TARGET/'src'/name,'-o',obj]);result.append(obj)
    return result

def libraries():
    return ['-L'+str(prefix()/'lib'),'-lminizip','-lcurl','-lmbedtls','-lmbedx509','-lmbedcrypto','-lz']
