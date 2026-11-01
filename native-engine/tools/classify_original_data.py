"""Conservative provenance review of data added by the port/compiler.

Original variable definitions remain ROM data. Compiler-generated switch
objects and variables absent from the original N64 preprocessing are separately
identified. This does not approve unresolved original data for publication.
"""
import concurrent.futures,json,re,subprocess,hashlib
from pathlib import Path
from collections import Counter,defaultdict
import build,build_probe
from rom_data_plan import unit_name

def main():
    report=json.loads((build.BUILD/'original-data-matches.json').read_text());types=json.loads((build.BUILD/'original-data-types/layouts.json').read_text())
    sources={Path(p).name:Path(p) for p in json.loads((build.TARGET/'source-files.json').read_text())};units={unit_name(r['owner']) for r in report['rows'] if not r.get('bank')};original={};errors={}
    compatibility=build.BUILD/'n64-preprocess';compatibility.mkdir(exist_ok=True)
    # IDO accepted token pasting around punctuation; GNU CPP rejects it.
    # Normalize that syntax in private copies without changing names/values.
    for name in ('bondconstants.h','bondaicommands.h'):
        text=(build.UP/'src'/name).read_text()
        text=re.sub(r'##\s*([()&,])',r'\1',text)
        text=re.sub(r'([()&,])\s*##',r'\1',text)
        (compatibility/name).write_text(text)
    args=build_probe.engine_args();args=[a for a in args if not str(a).startswith(('-DPORT=','-DPLATFORM_3DS=','-D__3DS__'))]
    def preprocess(unit):
        p=build.UP/sources[unit];r=subprocess.run([build.CC,'-I'+str(compatibility),*args,'-E','-P',p],capture_output=True,text=True)
        if r.returncode:
            # Original IDO metaprogramming need not expand for an absence
            # proof. This evaluates real #if branches and includes while
            # retaining unexpanded macro bodies, making absence conservative.
            r=subprocess.run([build.CC,'-I'+str(compatibility),*args,'-E','-P','-fdirectives-only',p],capture_output=True,text=True)
        return unit,r
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        for unit,result in pool.map(preprocess,sorted(units)):
            if result.returncode:errors[unit]=result.stderr
            else:original[unit]=result.stdout
    reviewed=[];unresolved=[]
    for row in report['rows']:
        if row.get('bank'):continue
        name=row['symbol'];base=name.split('.')[0];unit=unit_name(row['owner']);layout=types.get(unit,{}).get(base,{})
        kind=None;evidence=None
        if name.startswith('CSWTCH.'):
            kind='compiler-generated switch implementation';evidence='GCC synthesized object, not a source data definition'
        elif base.startswith('__func__'):
            kind='compiler-generated function-name diagnostic';evidence='compiler source identifier string'
        elif layout.get('file','').startswith('native_'):
            kind='native project implementation data';evidence=layout['file']
        elif unit in original and not re.search(r'\b'+re.escape(base)+r'\b',original[unit]):
            kind='port/native implementation data';evidence='identifier absent from original N64 preprocessing of '+str(sources[unit])
        if kind:reviewed.append({**row,'review':kind,'evidence':evidence})
        else:unresolved.append(row)
    out=dict(reviewed=reviewed,unresolved=unresolved,preprocessing_errors=errors)
    (build.BUILD/'original-data-provenance.json').write_text(json.dumps(out,indent=2)+'\n')
    print('Provenance review:',len(reviewed),'port/compiler objects;',len(unresolved),'original/unknown objects still require ROM loading; preprocessing errors',len(errors))
    print(Counter(r['review'] for r in reviewed));print([(r['symbol'],r['bytes']) for r in unresolved[:45]])
if __name__=='__main__':main()
