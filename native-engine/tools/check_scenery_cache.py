"""Exercise fixed-memory scenery attributes and stale-entry rejection."""
import build,build_probe

def main():
    out=build.BUILD/'scenery-cache-host';out.mkdir(exist_ok=True)
    binary=out/'check'
    build_probe.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(build.TARGET/'include'),build.TARGET/'tests/scenery_cache_host.cpp','-o',binary])
    import os
    env=dict(os.environ);env['ASAN_OPTIONS']='detect_leaks=0'
    print(build_probe.run([binary],env=env).stdout)
    binary=out/'batches'
    build_probe.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(build.TARGET/'include'),build.TARGET/'tests/scenery_batches_host.cpp','-o',binary])
    print(build_probe.run([binary],env=env).stdout)
    binary=out/'mesh-lod'
    build_probe.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(build.TARGET/'include'),build.TARGET/'tests/mesh_lod_host.cpp','-o',binary])
    print(build_probe.run([binary],env=env).stdout)
    binary=out/'clip-colour'
    build_probe.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(build.TARGET/'include'),build.TARGET/'tests/clip_colour_host.cpp','-o',binary])
    print(build_probe.run([binary],env=env).stdout)
    binary=out/'baked-mesh-lod'
    build_probe.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-D__3DS__','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(build.TARGET/'include'),'-I'+str(build.BUILD/'include'),build.TARGET/'tests/baked_mesh_lod_host.cpp','-o',binary])
    print(build_probe.run([binary,build.BUILD/'bg-lod/samples.bin'],env=env).stdout)

if __name__=='__main__':main()
