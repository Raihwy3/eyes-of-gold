#!/usr/bin/env python3
"""Force the original mainproc into a link to expose real bring-up dependencies.

This is a dependency audit, not a launchable full-engine target. No missing
functions or assets are stubbed to make this link pass.
"""
import json
import re
import subprocess
import build

def main():
    target=build.BUILD/'full-engine-link-audit.elf'
    args=[str(build.CC),'-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft',
          '-specs=3dsx.specs','-L/opt/devkitpro/libctru/lib',
          '-Wl,--gc-sections,--no-enum-size-warning,--undefined=mainproc',
          str(build.BUILD/'platform_3ds.o'),str(build.BUILD/'engine_probe.o'),
          str(build.BUILD/'native_ram.o'),str(build.BUILD/'native_cart.o'),str(build.BUILD/'game_memory.o'),str(build.BUILD/'input_3ds.o'),str(build.BUILD/'rom_fixups.o'),str(build.BUILD/'romassets.o'),str(build.BUILD/'runtime_symbols.o'),'-Wl,--start-group',str(build.BUILD/'liboriginal-engine.a'),str(build.BUILD/'libnative-os.a'),str(build.BUILD/'libnative-mixer.a'),str(build.BUILD/'libnative-baseline.a'),'-Wl,--end-group',
          '-lctru','-lm','-o',str(target)]
    result=subprocess.run(args,capture_output=True,text=True)
    log=result.stdout+result.stderr
    (build.BUILD/'full-engine-link.log').write_text(log)
    symbols=sorted(set(re.findall(r"undefined reference to [`‘]([^'’]+)['’]",log)))
    report={'original_entry':'mainproc','linked':result.returncode==0,
            'undefined_symbols':symbols,'note':'Audit only. The diagnostic main is not the game boot entry.'}
    (build.BUILD/'full-engine-link-report.json').write_text(json.dumps(report,indent=2)+'\n')
    # Even a future successful audit must not publish a diagnostic main as the
    # original game. The real boot target will have its own startup contract.
    if target.exists():target.unlink()
    print(f"Full-engine link: {'PASS' if report['linked'] else 'FAIL'}; {len(symbols)} unresolved symbols")
    print('Details:',build.BUILD/'full-engine-link-report.json')
    if result.returncode and not symbols:
        raise RuntimeError('Link audit failed unexpectedly:\n'+log)

if __name__=='__main__':main()
