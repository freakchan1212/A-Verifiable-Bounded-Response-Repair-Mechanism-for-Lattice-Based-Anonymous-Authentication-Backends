from __future__ import annotations
import argparse, csv
from pathlib import Path
from statistics import mean
from fullzk.generator import InstanceConfig, make_instance
from fullzk.bridge import run_backend

PROFILES={
    'natural_loose': lambda i,s: 'no_repair',
    'natural_medium': lambda i,s: 'repair_only' if ((i+s*17)%10==0) else 'no_repair',
    'natural_tight': lambda i,s: 'repair_only',
    'conditional_no_repair': lambda i,s: 'no_repair',
    'conditional_repair_only': lambda i,s: 'repair_only',
    'controlled_mixed_50_50': lambda i,s: 'mixed',
}

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--backend', choices=['fallback','lazer'], default='fallback'); ap.add_argument('--lazer-root', default='third_party/lazer'); ap.add_argument('--seeds', type=int, default=3); ap.add_argument('--sessions', type=int, default=20); ap.add_argument('--out', required=True); args=ap.parse_args()
    rows=[]
    for profile, fn in PROFILES.items():
        for seed in range(args.seeds):
            for sess in range(args.sessions):
                path=fn(sess,seed)
                obj=make_instance(InstanceConfig(seed_index=seed, session_index=sess, privacy_mode='committed', path_mode=path))
                r=run_backend(obj,args.backend,args.lazer_root)
                repair_flag=int(obj.get('repair_meta',{}).get('repair_flag',0))
                rows.append({'profile':profile,'seed':seed,'session':sess,'path_mode':path,'repair_flag':repair_flag,'verify_ok':r.get('verify_ok'), 'proof_ok':r.get('proof_ok'), 'proof_bytes':r.get('proof_bytes',0), 'prove_ms':r.get('prove_ms',0), 'verify_ms':r.get('verify_ms',0), 'total_ms':r.get('total_ms',0)})
    out=Path(args.out); out.parent.mkdir(parents=True, exist_ok=True)
    with out.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
             
    summ=[]
    for profile in PROFILES:
        rr=[r for r in rows if r['profile']==profile]
        vals=[float(r['total_ms']) for r in rr]
        vals_sorted=sorted(vals)
        p95=vals_sorted[int(0.95*(len(vals_sorted)-1))] if vals_sorted else 0
        summ.append({'workload_type':profile,'profile':profile,'rows':len(rr),'verify_ok':sum(str(r['verify_ok']).lower()=='true' for r in rr),'repair_rate':mean([int(r['repair_flag']) for r in rr]),'total_ms_mean':mean(vals) if vals else 0,'total_ms_p95':p95})
    sp=out.with_name(out.stem+'_summary.csv')
    with sp.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f, fieldnames=list(summ[0].keys())); w.writeheader(); w.writerows(summ)
    print(out); print(sp)
if __name__=='__main__': main()
