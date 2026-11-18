#!/usr/bin/env python3
"""Summarize diagnostic worker wall times; does not estimate hardware CPU use."""
import argparse,json,re
from pathlib import Path
PATTERN=re.compile(r'Worker timing: stage=([\w-]+) core=(\d+) windowMs=([\d.]+) samples=(\d+) avgMs=([\d.]+) peakMs=([\d.]+) spanShare=([\d.]+)%')
def summarize(text):
    rows={}
    for match in PATTERN.finditer(text):
        stage,core,span,count,avg,peak,share=match.groups()
        span,avg,peak,share=map(float,(span,avg,peak,share));count=int(count)
        if span<=0 or count<=0:continue
        row=rows.setdefault((stage,int(core)),dict(stage=stage,core=int(core),windows=0,samples=0,total_ms=0.,span_ms=0.,peak_ms=0.))
        row['windows']+=1;row['samples']+=count;row['total_ms']+=avg*count
        row['span_ms']+=span;row['peak_ms']=max(row['peak_ms'],peak)
    result=[]
    for row in rows.values():
        row['average_ms']=row['total_ms']/row['samples']
        row['wall_span_share_percent']=100.*row['total_ms']/row['span_ms']
        result.append(row)
    return sorted(result,key=lambda r:(r['core'],r['stage']))
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('log',type=Path);p.add_argument('--json',action='store_true');args=p.parse_args()
    text=args.log.read_text(errors='replace');rows=summarize(text)
    if not rows:raise SystemExit('No completed Worker timing windows found. Use the timing build with debug-log.flag and play for at least 30 seconds.')
    if args.json:print(json.dumps(rows,indent=2));return
    print('Wall times include preemption. Nested rows overlap; do not add them. These are not CPU-utilization percentages.')
    print('Core  Stage             Samples  Avg ms  Peak ms  Wall span %')
    for r in rows:print(f"{r['core']:4}  {r['stage']:17} {r['samples']:7} {r['average_ms']:7.2f} {r['peak_ms']:8.2f} {r['wall_span_share_percent']:11.1f}")
    print('Render-work includes FPS pacing, GPU waits and submission; inspect those rows and existing previousGPU/Render window lines separately.')
if __name__=='__main__':main()
