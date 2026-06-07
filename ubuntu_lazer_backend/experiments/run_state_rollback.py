from __future__ import annotations
import argparse, copy, csv
from pathlib import Path
from fullzk.generator import InstanceConfig, make_instance
from fullzk.fullrelation import sha256_fullrelation_check
from fullzk.bridge import run_backend
from experiments.run_fullrelation_sha256_smoke import add_v2_digest_fields

def tamper_hex(x): return ('ff' if not str(x).startswith('ff') else '00') + str(x)[2:]

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--backend', choices=['fallback','lazer'], default='fallback'); ap.add_argument('--lazer-root', default='third_party/lazer'); ap.add_argument('--out', required=True); args=ap.parse_args()
    old=add_v2_digest_fields(make_instance(InstanceConfig(epoch=0, session_index=0, privacy_mode='committed', path_mode='mixed')))
    new=add_v2_digest_fields(make_instance(InstanceConfig(epoch=1, session_index=1, privacy_mode='committed', path_mode='mixed')))
    cases=[]
    def add(name, obj): cases.append((name,obj))
    add('honest_new_epoch', copy.deepcopy(new))
    o=copy.deepcopy(new); o['statement']['refresh_state']=old['statement']['refresh_state']; add('old_refresh_state_new_epoch', o)
    o=copy.deepcopy(new); o['statement']['refresh_tag']=old['statement']['refresh_tag']; add('old_refresh_tag_new_epoch', o)
    o=copy.deepcopy(new); o['statement']['nullifier']=old['statement']['nullifier']; add('old_nullifier_new_session', o)
    o=copy.deepcopy(new); o['public_inputs']['scope']='other-scope'; add('cross_scope_replay', o)
    rows=[]
    for name,obj in cases:
        sha=sha256_fullrelation_check(obj)
        try:
            r=run_backend(obj,args.backend,args.lazer_root)
            accepted=bool(r.get('verify_ok')) and bool(sha.get('sha256_fullrelation_ok'))
            failed=';'.join(r.get('failed_checks',[]))+';'+ ';'.join(k for k,v in sha.get('checks',{}).items() if not v)
        except Exception as e:
            accepted=False; failed='backend_exception:'+type(e).__name__
        expected=(name=='honest_new_epoch')
        rows.append({'case':name,'accepted':accepted,'expected_accept':expected,'pass':accepted==expected,'failed_checks':failed})
    out=Path(args.out); out.parent.mkdir(parents=True, exist_ok=True)
    with out.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
    print(out)
    if any(not r['pass'] for r in rows): raise SystemExit(2)
if __name__=='__main__': main()
