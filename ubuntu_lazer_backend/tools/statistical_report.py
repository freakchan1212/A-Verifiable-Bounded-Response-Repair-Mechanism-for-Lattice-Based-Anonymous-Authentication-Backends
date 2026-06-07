from __future__ import annotations
import argparse, csv, json, math
from pathlib import Path
from statistics import mean, median

def wilson(k:int,n:int,z:float=1.96):
    if n==0: return (0.0,1.0)
    ph=k/n; den=1+z*z/n; c=(ph+z*z/(2*n))/den; half=z*math.sqrt((ph*(1-ph)+z*z/(4*n))/n)/den
    return max(0.0,c-half), min(1.0,c+half)

def q(vals, quant):
    if not vals: return 0.0
    vals=sorted(vals); idx=min(len(vals)-1, int(math.ceil(quant*len(vals))-1)); return vals[idx]

def read_csv(path: Path):
    if not path.exists(): return []
    return list(csv.DictReader(path.open(encoding='utf-8')))

def boolval(x): return str(x).lower()=='true'

def summarize_batch(rows):
    if not rows: return {}
    n=len(rows); out={'rows':n}
    for col in ['verify_ok','proof_ok','relation_ok','repair_ok','binding_ok','sha256_fullrelation_ok']:
        if col in rows[0]:
            k=sum(boolval(r.get(col,'')) for r in rows); lo,hi=wilson(k,n)
            out[col]={'count':k,'n':n,'rate':k/n,'wilson95':[lo,hi]}
    for col in ['proof_bytes','prove_ms','verify_ms','total_ms']:
        if col in rows[0]:
            vals=[float(r[col]) for r in rows if str(r.get(col,'')) not in ('','None')]
            out[col]={'mean':mean(vals),'median':median(vals),'p95':q(vals,0.95),'p99':q(vals,0.99)} if vals else {}
    return out

def summarize_tamper(rows):
    if not rows: return {}
    honest=[r for r in rows if r.get('case')=='honest']
    attacks=[r for r in rows if r.get('case')!='honest']
    rej=sum(not boolval(r.get('verify_ok','')) for r in attacks)
    lo,hi=wilson(rej,len(attacks)) if attacks else (0,1)
    return {'honest_rows':len(honest),'attack_rows':len(attacks),'attack_rejected':rej,'attack_reject_rate':(rej/len(attacks) if attacks else 0),'wilson95':[lo,hi]}

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--artifact-dir', required=True); ap.add_argument('--out-md', required=True); ap.add_argument('--out-json', required=True); args=ap.parse_args()
    root=Path(args.artifact_dir)
    candidates=[root/'batch.csv', root/'system_lazer_committed_mixed'/'batch.csv', root/'v2_sha256_authrelation_lazer_strict'/'batch.csv']
    batch=[]
    for p in candidates:
        batch=read_csv(p)
        if batch: break
    tamper=[]
    for name in ['adversarial_summary.csv','combo_tamper_summary.csv','multi_native_splice_summary.csv']:
        p=root/name
        if p.exists(): tamper.extend(read_csv(p))
    data={'batch':summarize_batch(batch),'tamper':summarize_tamper(tamper)}
    Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out_json).write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding='utf-8')
    md=['# Statistical report','','## Batch results','']
    if data['batch']:
        for k,v in data['batch'].items():
            md.append(f'- **{k}**: `{v}`')
    else:
        md.append('No batch CSV found in this artifact directory.')
    md += ['','## Adversarial / robustness rejection','']
    md.append(str(data['tamper'] or 'No tamper CSV found.'))
    md += ['','## Interpretation boundary','','Wilson intervals are used as lightweight reporting intervals. They do not replace a full cryptographic security proof. Session-level counts are complemented by seed-level summaries when seed identifiers are present.']
    Path(args.out_md).write_text('\n'.join(md)+'\n', encoding='utf-8')
    print(args.out_md)
if __name__=='__main__': main()
