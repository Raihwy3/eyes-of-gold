#!/usr/bin/env python3
"""Check the real console/GPU handoff against the libctru display contract."""
import build
import build_probe


def main():
    out=build.BUILD/'display-host';out.mkdir(parents=True,exist_ok=True)
    binary=out/'display-check'
    args=['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror',
          '-I'+str(build.TARGET/'tests/display'),'-I'+str(build.TARGET/'include'),
          build.TARGET/'src/display_3ds.c',build.TARGET/'tests/display/check.c','-o',binary]
    build_probe.run(args)
    result=build_probe.run([binary])
    (build.BUILD/'display-host-validation.log').write_text(result.stdout+result.stderr)
    print(result.stdout)

if __name__=='__main__':main()
