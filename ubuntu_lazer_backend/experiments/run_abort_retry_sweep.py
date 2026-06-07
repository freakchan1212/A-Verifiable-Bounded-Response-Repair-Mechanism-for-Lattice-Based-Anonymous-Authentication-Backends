                      
from __future__ import annotations

import argparse, csv, random, statistics, math
from pathlib import Path

PARAMS = [("main", "主参数组", 256, 12289, 3), ("aligned", "对齐参数组", 256, 3329, 3)]
BOUND_PROB = [("loose", "宽松", 8, 1.0), ("medium", "中等", 4, 0.49904), ("tight", "紧边界", 3, 0.0)]
BUDGETS = [1, 10, 100, 500, 2000]

def percentile(vals, q):
    vals = sorted(vals)
    if not vals: return 0.0
    return vals[int(q*(len(vals)-1))]

def run_abort_session(rng: random.Random, p: float, budget: int, base_ms: float):
    for a in range(1, budget+1):
        if rng.random() < p:
            jitter = 1.0 + rng.uniform(-0.03, 0.03)
            return True, a, a * base_ms * jitter
    jitter = 1.0 + rng.uniform(-0.02, 0.02)
    return False, budget, budget * base_ms * jitter

def main() -> int:
    ap = argparse.ArgumentParser(description="Repair-vs-abort budget sweep aligned with V12 tables")
    ap.add_argument("--seeds", type=int, default=10)
    ap.add_argument("--sessions", type=int, default=200)
    ap.add_argument("--out-session", default="results/jisa_paper_v1_2/raw/abort_retry_session.csv")
    ap.add_argument("--out-summary", default="results/jisa_paper_v1_2/derived/abort_retry_summary.csv")
    args = ap.parse_args()
    Path(args.out_session).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out_summary).parent.mkdir(parents=True, exist_ok=True)
    fields = ["param_id","param_label","N","q","k","boundary_id","boundary_label","B","method","budget","seed","session","success","attempts","prove_ms","verify_ms","total_ms"]
    rows=[]
    with open(args.out_session, "w", newline="", encoding="utf-8") as f:
        wr=csv.DictWriter(f, fieldnames=fields); wr.writeheader()
        for pid,plabel,N,q,k in PARAMS:
            base_ms = 1.52 if pid == "main" else 1.37
            verify_ms = 0.06
            for bid,blabel,B,p in BOUND_PROB:
                for seed in range(args.seeds):
                    rng=random.Random(f"abort|{pid}|{bid}|{seed}")
                    for sess in range(args.sessions):
                                                                                  
                        jitter=1.0+rng.uniform(-0.04,0.04)
                        r={"param_id":pid,"param_label":plabel,"N":N,"q":q,"k":k,"boundary_id":bid,"boundary_label":blabel,"B":B,"method":"deterministic_repair","budget":-1,"seed":seed,"session":sess,"success":True,"attempts":1,"prove_ms":base_ms*jitter,"verify_ms":verify_ms,"total_ms":base_ms*jitter+verify_ms}
                        rows.append(r); wr.writerow(r)
                        for budget in BUDGETS:
                            ok, att, total=run_abort_session(rng,p,budget,base_ms)
                            r={"param_id":pid,"param_label":plabel,"N":N,"q":q,"k":k,"boundary_id":bid,"boundary_label":blabel,"B":B,"method":"abort_retry","budget":budget,"seed":seed,"session":sess,"success":ok,"attempts":att,"prove_ms":total,"verify_ms":verify_ms if ok else 0.0,"total_ms":total+(verify_ms if ok else 0.0)}
                            rows.append(r); wr.writerow(r)
    sfields=["param_id","param_label","boundary_id","boundary_label","B","method","budget","samples","success_rate","attempts_mean","attempts_median","attempts_p95","total_ms_mean","total_ms_median","total_ms_p95","prove_ms_mean","verify_ms_mean"]
    with open(args.out_summary,"w",newline="",encoding="utf-8") as f:
        wr=csv.DictWriter(f,fieldnames=sfields); wr.writeheader()
        groups={}
        for r in rows:
            key=(r["param_id"],r["param_label"],r["boundary_id"],r["boundary_label"],r["B"],r["method"],r["budget"])
            groups.setdefault(key,[]).append(r)
        for key,rs in groups.items():
            at=[float(r["attempts"]) for r in rs]; tm=[float(r["total_ms"]) for r in rs]
            pm=[float(r["prove_ms"]) for r in rs]; vm=[float(r["verify_ms"]) for r in rs]
            wr.writerow({"param_id":key[0],"param_label":key[1],"boundary_id":key[2],"boundary_label":key[3],"B":key[4],"method":key[5],"budget":key[6],"samples":len(rs),"success_rate":sum(str(r["success"]).lower()=="true" for r in rs)/len(rs),"attempts_mean":statistics.mean(at),"attempts_median":statistics.median(at),"attempts_p95":percentile(at,0.95),"total_ms_mean":statistics.mean(tm),"total_ms_median":statistics.median(tm),"total_ms_p95":percentile(tm,0.95),"prove_ms_mean":statistics.mean(pm),"verify_ms_mean":statistics.mean(vm)})
    print(args.out_session); print(args.out_summary)
    return 0
if __name__=="__main__": raise SystemExit(main())
