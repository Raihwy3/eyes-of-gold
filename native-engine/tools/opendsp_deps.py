"""Build the pinned OpenDSP library in-tree; never install over devkitPro."""
import os,json,hashlib
import build,build_probe
VENDOR=build.TARGET/'vendor/OpenDSP'
def library():
    pins=json.loads((VENDOR/'source.json').read_text())['files']
    for name,digest in pins.items():
        if hashlib.sha256((VENDOR/name).read_bytes()).hexdigest()!=digest:raise RuntimeError('OpenDSP source changed: '+name)
    result=build_probe.run(['make','-C',VENDOR/'library','-j4'],env={**os.environ,'DEVKITPRO':'/opt/devkitpro'})
    return VENDOR/'library/lib/libopendsp.a'
def include():return VENDOR/'library/include'
