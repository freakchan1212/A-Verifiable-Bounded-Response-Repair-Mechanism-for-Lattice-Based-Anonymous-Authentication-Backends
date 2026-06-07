from __future__ import annotations
import argparse, copy, csv, json
from pathlib import Path
from fullzk.generator import InstanceConfig, make_instance
from fullzk.fullrelation import sha256_fullrelation_check
from fullzk.bridge import run_backend
from experiments.run_fullrelation_sha256_smoke import add_v2_digest_fields

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--backend', choices=['fallback','lazer'], default='fallback'); ap.add_argument('--lazer-root', default='third_party/lazer'); ap.add_argument('--out', required=True); args=ap.parse_args()
    base=add_v2_digest_fields(make_instance(InstanceConfig(privacy_mode='committed', path_mode='mixed')))
    cases=[]
    def add(name, mut):
        obj=copy.deepcopy(base); err=''
        try: mut(obj)
        except Exception as e: err=str(e)
        cases.append((name,obj,err))
    add('missing_ctx_digest', lambda o: o['statement'].pop('ctx_digest', None))
    add('empty_refresh_tag', lambda o: o['statement'].__setitem__('refresh_tag',''))
    add('truncated_repair_digest', lambda o: o['statement'].__setitem__('repair_digest', o['statement']['repair_digest'][:16]))
    add('bad_hex_nullifier', lambda o: o['statement'].__setitem__('nullifier','zz'+o['statement']['nullifier'][2:]))
    add('wrong_type_z', lambda o: o['witness'].__setitem__('z_repaired','not-a-vector'))
    rows=[]
    for name,obj,err in cases:
        sha_ok=False; verify=False; proof_ok=False; fail=''
        try:
            sha=sha256_fullrelation_check(obj); sha_ok=bool(sha.get('sha256_fullrelation_ok'))
        except Exception as e:
            fail='sha_exception:'+type(e).__name__
        if not fail:
            try:
                r=run_backend(obj,args.backend,args.lazer_root); verify=bool(r.get('verify_ok')) and sha_ok; proof_ok=bool(r.get('proof_ok')); fail=';'.join(r.get('failed_checks',[]))
            except Exception as e:
                fail='backend_exception:'+type(e).__name__
        rows.append({'case':name,'accepted':verify,'proof_ok':proof_ok,'sha256_fullrelation_ok':sha_ok,'expected_accept':False,'pass':not verify,'failure':fail or err})
    out=Path(args.out); out.parent.mkdir(parents=True, exist_ok=True)
    with out.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
    print(out)
    if any(r['accepted'] for r in rows): raise SystemExit(2)
if __name__=='__main__': main()
