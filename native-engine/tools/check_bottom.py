import os
import build,build_probe

def main():
    import bottom_background
    bottom_background.generate()
    out=build.BUILD/'bottom-host';out.mkdir(exist_ok=True)
    flags=['-fsanitize=address,undefined','-g','-fno-omit-frame-pointer'] if os.environ.get('EOG_BOTTOM_SANITIZE') else []
    binary=out/('check-sanitize' if flags else 'check')
    build_probe.run(['gcc','-std=gnu11','-O1','-Wall','-Wextra','-Werror',*flags,'-I'+str(build.TARGET/'tests/bottom/mock'),'-I'+str(build.TARGET/'include'),'-I'+str(build.BUILD/'include'),build.TARGET/'tests/bottom/check.c','-pthread','-lm','-o',binary])
    result=build_probe.run([binary],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
    print(result.stdout)
if __name__=='__main__':main()
