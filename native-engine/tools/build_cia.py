#!/usr/bin/env python3
"""Package a standalone CIA from the native engine, sharing its SD data folder."""
from pathlib import Path
import os,shutil,subprocess,re,json,hashlib,struct,wave,zipfile
import build,build_probe

def tool(name,variable):
    value=os.environ.get(variable) or shutil.which(name)
    if not value:
        local=build.ROOT/'local/cia-tools'/name
        if local.is_file():value=str(local)
    if not value:raise RuntimeError('Install '+name+' or set '+variable+' to its executable. See docs/cia.md.')
    return value

def title_version(version):
    """Monotonic 16-bit installed version, including the release revision.

    2 major / 6 minor / 4 patch / 4 revision bits. Older CIA builds used
    only the base version (151 for 0.9.7), so this also advances that series.
    """
    match=re.fullmatch(r'(\d+)\.(\d+)\.(\d+)(?:-r(\d+))?',version)
    if not match:raise ValueError('Unsupported CIA release version: '+version)
    major,minor,patch,revision=(int(value or 0) for value in match.groups())
    if major>3 or minor>63 or patch>15 or revision>15:
        raise ValueError('CIA version exceeds the installed metadata field limits')
    return (major<<14)|(minor<<8)|(patch<<4)|revision


def banner(out):
    from PIL import Image,ImageDraw,ImageFont
    image=Image.new('RGBA',(256,128),(0,0,0,0));draw=ImageDraw.Draw(image)
    icon=Image.open(build.TARGET/'cia/eye-transparent.png').convert('RGBA')
    assert icon.getextrema()[3][0]==0, 'Banner artwork must retain transparency'
    icon=icon.crop(icon.getbbox());icon.thumbnail((110,81),Image.Resampling.LANCZOS)
    image.alpha_composite(icon,((256-icon.width)//2,3))
    # Host script lettering, independently generated; no game font is bundled.
    font_path=Path(os.environ.get('EOG_BANNER_FONT','/usr/share/fonts/gsfonts/Z003-MediumItalic.otf'))
    if not font_path.is_file():raise RuntimeError('Set EOG_BANNER_FONT to an installed script font.')
    font=ImageFont.truetype(str(font_path),34)
    draw.text((129,92),'Eyes of Gold',font=font,fill=(104,67,15,255),anchor='mt')
    draw.text((128,91),'Eyes of Gold',font=font,fill=(234,193,78,255),anchor='mt')
    image.save(out/'banner.png')
    with wave.open(str(out/'silence.wav'),'wb') as f:f.setparams((1,2,32000,0,'NONE','not compressed'));f.writeframes(bytes(6400))

def validate(cia,elf,smdh,bnr):
    from elftools.elf.elffile import ELFFile
    raw=cia.read_bytes();header,kind,version,cert,ticket,tmd,meta,content=struct.unpack_from('<IHHIIIIQ',raw)
    align=lambda x:(x+63)&~63
    at=align(header)+align(cert)+align(ticket)+align(tmd)
    tmd_at=align(header)+align(cert)+align(ticket)
    signature=struct.unpack_from('>I',raw,tmd_at)[0]
    signature_bytes={0x10000:0x240,0x10001:0x140,0x10002:0x80,0x10003:0x240,0x10004:0x140,0x10005:0x80}[signature]
    installed_version=struct.unpack_from('>H',raw,tmd_at+signature_bytes+0x9c)[0]
    version=re.search(r'NATIVE_ENGINE_VERSION "([^\"]+)"',(build.TARGET/'include/native_version.h').read_text()).group(1)
    assert installed_version==title_version(version), 'CIA metadata version mismatch; use a corrected makerom build'
    ncch=raw[at:at+content];assert ncch[0x100:0x104]==b'NCCH'
    title=struct.unpack_from('<Q',ncch,0x118)[0];assert title==0x000400000E064D00
    assert ncch[0x18f]&4 # Unencrypted homebrew, so payload checks are possible.
    assert struct.unpack_from('<II',ncch,0x198)==(0,0) # Logo is stored in ExeFS.
    assert struct.unpack_from('<II',ncch,0x1b0)==(0,0) # No bundled game-data RomFS.
    exefs_at,exefs_blocks=struct.unpack_from('<II',ncch,0x1a0);exefs=ncch[exefs_at*512:(exefs_at+exefs_blocks)*512];entries={}
    for i in range(10):
        name,offset,size=struct.unpack_from('<8sII',exefs,i*16);name=name.rstrip(b'\0').decode()
        if name:
            data=exefs[512+offset:512+offset+size];assert len(data)==size
            assert hashlib.sha256(data).digest()==exefs[0xc0+(9-i)*32:0xc0+(10-i)*32]
            entries[name]=data
    assert entries['icon']==smdh.read_bytes() and entries['banner']==bnr.read_bytes()
    assert set(entries)=={'.code','icon','banner','logo'}, 'Missing HOME Menu boot logo'
    assert hashlib.sha256(entries['logo']).hexdigest()=='62a5a1f9091aefb46b52e31fbeca2fdba9a99fe2473237e21e35b8d2e5659dff', 'Boot logo does not match verified makerom Homebrew logo'
    with elf.open('rb') as f:
        e=ELFFile(f);loads=[p for p in e.iter_segments() if p['p_type']=='PT_LOAD'];base=min(p['p_vaddr'] for p in loads)
        for p in loads:
            offset=p['p_vaddr']-base;assert entries['.code'][offset:offset+p['p_filesz']]==p.data()
    assert b'sdmc:/3ds/eyes-of-gold-native-game' in entries['.code']
    assert b'LAUNCHER UPDATE' in entries['.code'] and b'launcher-update.cia' in entries['.code']
    return {'status':'passed','installed_title_version':installed_version,'title_id':f'{title:016X}','bytes':len(raw),'exefs_files':sorted(entries),'elf_segments_match':True,'same_sd_folder':True,'romfs_bundled':False,'boot_logo':'makerom Homebrew (verified SHA-256)','console_tested':False}

def main():
    makerom=tool('makerom','MAKEROM');bannertool=tool('bannertool','BANNERTOOL')
    baseline=build.BUILD/'eyes-of-gold-native-game'
    if not baseline.with_suffix('.map').exists():raise RuntimeError('Build the native game first with make, or run make cia.')
    build.prepare();out=build.BUILD/'cia';out.mkdir(exist_ok=True)
    banner(out);bnr=out/'banner.bnr';smdh=baseline.with_suffix('.smdh')
    model=build.TARGET/'cia/banner.cgfx';model_bytes=model.read_bytes()
    assert model_bytes[:4]==b'CGFX' and len(model_bytes)<=0x80000
    build_probe.run([bannertool,'makebanner','-ci',model,'-a',out/'silence.wav','-o',bnr])
    arch=['-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft'];platform=out/'platform.o'
    build_probe.run([build.CC,*arch,'-std=gnu11','-O2','-Wall','-Wextra','-Werror','-D__3DS__','-DNATIVE_CIA_BUILD','-I'+str(build.TARGET/'include'),'-I'+str(build.BUILD/'include'),'-I/opt/devkitpro/libctru/include','-c',build.TARGET/'src/native_game_platform.c','-o',platform])
    objects=[]
    for name in re.findall(r'^LOAD (.+\.o)$',baseline.with_suffix('.map').read_text(),re.M):
        if not name.startswith(str(build.BUILD)):continue
        objects.append(platform if name==str(build.BUILD/'game/platform.o') else Path(name))
    assert platform in objects
    import opendsp_deps,updater_deps
    opendsp_library=opendsp_deps.library()
    target=build.BUILD/'eyes-of-gold-launcher-cia'
    build_probe.run([build.CC.with_name('arm-none-eabi-g++'),*arch,'-specs=3dsx.specs','-L/opt/devkitpro/libctru/lib','-Wl,--gc-sections,--no-enum-size-warning,-Map,'+str(target.with_suffix('.map')),*objects,'-Wl,--start-group',*[build.BUILD/name for name in ('liboriginal-engine.a','liboriginal-rsp.a','libnative-os.a','libnative-mixer.a','libnative-baseline.a')],'-Wl,--end-group',*updater_deps.libraries(),opendsp_library,'-lcitro3d','-lctru','-lm','-o',target.with_suffix('.elf')])
    import audit_original_data
    audit_original_data.require_clean_release(target.with_suffix('.elf'))
    shutil.copyfile(build.BUILD/'strict-release-data-audit.json',out/'strict-release-data-audit.json')
    version=re.search(r'NATIVE_ENGINE_VERSION "([^"]+)"',(build.TARGET/'include/native_version.h').read_text()).group(1)
    cia=target.with_suffix('.cia')
    result=build_probe.run([makerom,'-f','cia','-o',cia,'-elf',target.with_suffix('.elf'),'-rsf',build.TARGET/'cia/launcher.rsf','-icon',smdh,'-banner',bnr,'-exefslogo','-target','t','-ver',str(title_version(version))])
    print(result.stdout,result.stderr)
    report=validate(cia,target.with_suffix('.elf'),smdh,bnr);report['version']=version;report['banner']={'format':'CGFX','geometry_3d':True,'bytes':len(model_bytes),'sha256':hashlib.sha256(model_bytes).hexdigest(),'microphone_console_tested':False};report['sha256']=hashlib.sha256(cia.read_bytes()).hexdigest()
    (out/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
    release=build.ROOT/'local/releases/cia';release.mkdir(exist_ok=True)
    name='eyes-of-gold-launcher-'+version+'.cia';shutil.copy2(cia,release/name)
    readme='''Eyes of Gold Launcher — CIA test build

Install the .cia with FBI, then launch it from HOME Menu.
It uses /3ds/eyes-of-gold-native-game/ on the SD card for the ROM,
saves, native-options.ini and all prepared game files. Keep that folder.
You do not need to move or reconvert an existing 3DSX installation.
For a fresh installation, put your legally backed-up US retail ROM in that
folder, named GoldenEye.n64, GoldenEye.z64 or GoldenEye.v64.

CIA updates are downloaded by the built-in updater as
/3ds/eyes-of-gold-native-game/launcher-update.cia when a newer release
includes a .cia asset. Close the game and install that file with FBI.
The existing 3DSX version and its updater continue to work separately.

Title ID: 000400000E064D00. This package contains the launcher and its
custom HOME Menu artwork. No ROM or extracted game assets are included.
The package passed host structure, payload and asset checks; it still
needs a console boot test.
'''
    (release/'INSTALL.txt').write_text(readme)
    with zipfile.ZipFile(release/('eyes-of-gold-cia-'+version+'.zip'),'w',zipfile.ZIP_DEFLATED) as z:
        z.write(release/name,name);z.write(release/'INSTALL.txt','INSTALL.txt');z.write(build.TARGET/'cia/makerom-logo-LICENSE.txt','licenses/makerom-logo.txt')
        for source in (build.TARGET/'distribution').glob('*.txt'):
            if source.name!='GAME-README.txt':z.write(source,source.name)
        for source in (build.TARGET/'distribution/licenses').glob('*.txt'):z.write(source,'licenses/'+source.name)
        for file,label in ((build.UP/'LICENSE','upstream-LICENSE.txt'),(build.UP/'NOTICE','upstream-NOTICE.txt'),(build.UP/'port/fast3d/LICENSE.txt','fast3d-LICENSE.txt')):z.write(file,label)
    print(json.dumps(report,indent=2));print('CIA package:',release/('eyes-of-gold-cia-'+version+'.zip'))
if __name__=='__main__':main()
