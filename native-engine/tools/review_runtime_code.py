"""Record explicit code/state reviews after source and literal-pool inspection.

Not run by packaging. Unknown symbols/sections receive no approval. These are
engine control state, standard codec/printf implementations and independently
added diagnostics/UI text, never original game-content records.
"""
import hashlib,json
from pathlib import Path
from elftools.elf.elffile import ELFFile
import build
from rom_data_plan import unit_name
from review_port_data import normalizer
from audit_original_data import inventory,anonymous_inventory

POOL_REVIEWS={
('boss.c','.data.eog_rom_strings_df862bba1b3a874d'):'Native boot progress and command-line formatting added by the port.',
('boss.c','.data.eog_rom_strings_947e6b9361d4feae'):'Port/native diagnostics, supported-stage error and sampled performance logging.',
('memp.c','.data.eog_rom_strings_d641532124f11336'):'Port allocation-redzone diagnostics.',
('front.c','.data.eog_rom_strings_fc4fda9a92c8e054'):'Custom Exit entry; original difficulty names use ROM language lookup.',
('prop.c','.data.eog_rom_strings_eaa08453bda857c9'):'Native asset-loading progress diagnostics.',
('bg.c','.data.eog_rom_strings_dd216eb2c4ba7963'):'Port visibility/memory diagnostics.',
('file2.c','.data.eog_rom_strings_d23807f47c762f41'):'Port save-validation diagnostics; original default save records are ROM-backed.',
('image.c','.data.eog_rom_strings_e1eed3a5984231df'):'Port texture-decoder diagnostics.',
('initmttex.c','.data.eog_rom_strings_9c71d7238803fd4d'):'Port texture-pool diagnostics and token-present status.',
('options.c','.data.eog_rom_strings_d3f3dbeb75408f82'):'Custom Experimental/Audio/Game Options/Advanced section headings; original string storage remains zero.',
('options.c','.data.eog_rom_strings_40a9d37ad92ea36d'):'Custom CPU/DSP mixer choices, independently added 5:4 ratio label and guard Hz format; remaining bytes are zero imported-ROM string storage/padding.',
('options.c','.rodata'):'Compiler-created local pointer arrays for custom watch/renderer setting choices in native_options_bridge.h; original choices/names use ROM data.',
('propobj.c','.data.eog_rom_strings_f1252c1b85ee5c42'):'Port door-sound slot diagnostics.',
('propobj.c','.data.eog_rom_strings_4a025fdb850306a9'):'Native LOD/view-distance diagnostic logging.',
('csplayer.c','.data.eog_rom_strings_c845b4e8097ad74b'):'Port music tracing and compiler assertion/file diagnostics.',
('save.c','.data.eog_rom_strings_d92a61151aa80866'):'Compiler assertion/file diagnostics for audio implementation.',
('seqplayer.c','.data.eog_rom_strings_125c2a2096e47acf'):'Compiler assertion/file diagnostics for audio implementation.',
('synthesizer.c','.data.eog_rom_strings_7e18b57a99b37c60'):'Compiler assertion/file diagnostics for audio implementation.',
('reverb.c','.data.eog_rom_strings_e91e1c8f7e98f580'):'Compiler source-file identifier for audio assertion diagnostics.'
}


def main():
    elf=build.BUILD/'eyes-of-gold-native-game.elf';policy_path=build.TARGET/'rom-data-code-review.json';policy=json.loads(policy_path.read_text());approved={(r['unit'],r['symbol']):r for r in policy['rows']};spec=json.loads((build.TARGET/'engine-state-review.json').read_text());scopes={r['symbol']:r for r in spec['rows']};sources={p.name:build.UP/p for p in build.source_files()};new=[]
    # Pin the reviewed code-generator/header inputs too. Relocation-normalized
    # hashes alone cannot distinguish two different pointer-table orderings.
    files=[build.TARGET/'tools/build.py',*sorted(build.TARGET.joinpath('include').glob('native_*.h'))]
    pins={str(p.relative_to(build.ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    with elf.open('rb') as f:
        e=ELFFile(f);get=normalizer(e)
        for row in inventory(elf):
            name=row['symbol'];unit=unit_name(row['owner']);scope=scopes.get(name)
            if scope is None:continue
            assert unit in sources,'Code state lacks a source unit'
            source=sources[unit];text=source.read_text();assert name.startswith('CSWTCH.') or name.split('.')[0] in text,(name,'source definition missing')
            record=dict(unit=unit,symbol=name,bytes=row['bytes'],sha256=hashlib.sha256(get(row['address'],row['bytes'])).hexdigest(),review=scope['review'],evidence=scope['evidence'],source_sha256={**pins,str(source.relative_to(build.ROOT)):hashlib.sha256(source.read_bytes()).hexdigest()})
            approved[(unit,name)]=record;new.append(name)
        pool_rows=[]
        for row in anonymous_inventory(elf):
            key=(unit_name(row['owner']),row['section']);evidence=POOL_REVIEWS.get(key)
            if evidence is None:continue
            pool_rows.append(dict(unit=key[0],section=key[1],bytes=row['bytes'],sha256=hashlib.sha256(get(row['address'],row['bytes'])).hexdigest(),review='reviewed code diagnostics or custom UI implementation',evidence=evidence,source_sha256=pins))
    policy['rows']=list(approved.values());policy_path.write_text(json.dumps(policy,indent=2)+'\n')
    (build.TARGET/'rom-code-pool-review.json').write_text(json.dumps(dict(version=1,upstream=build.UPSTREAM_REV,rows=pool_rows),indent=2)+'\n')
    print('Pinned explicit runtime/code reviews:',len(new),'objects;',len(pool_rows),'diagnostic/custom-UI pools')
if __name__=='__main__':main()
