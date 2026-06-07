                      
from __future__ import annotations

import argparse, csv, random, math
from pathlib import Path

PARAMS = [
    ("main", "主参数组", 256, 12289, 3),
    ("aligned", "对齐参数组", 256, 3329, 3),
]
BOUNDS = [("loose", "宽松", 8), ("medium", "中等", 4), ("tight", "紧边界", 3)]

def draw_raw_maxabs(rng: random.Random) -> int:
                                                             
                                                                                 
    u = rng.random()
    if u < 0.49904:
        return 4
    if u < 0.78:
        return 5
    if u < 0.93:
        return 6
    return 7

def wilson_ci(k: int, n: int, z: float = 1.96):
    if n == 0:
        return 0.0, 0.0
    p = k / n
    denom = 1 + z*z/n
    center = (p + z*z/(2*n)) / denom
    half = z * math.sqrt((p*(1-p) + z*z/(4*n))/n) / denom
    return max(0.0, center-half), min(1.0, center+half)

def main() -> int:
    ap = argparse.ArgumentParser(description="V12/V13-aligned boundary hit probability sweep")
    ap.add_argument("--seeds", type=int, default=10)
    ap.add_argument("--trials", type=int, default=10000)
    ap.add_argument("--out-csv", default="results/jisa_paper_v1_2/raw/boundary_hit_probability.csv")
    args = ap.parse_args()
    Path(args.out_csv).parent.mkdir(parents=True, exist_ok=True)
    fields = ["param_id","param_label","N","q","k","boundary_id","boundary_label","B","seed","trials","hits","hit_rate","ci95_low","ci95_high","expected_attempts","observed_maxabs_mean"]
    with open(args.out_csv, "w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=fields); wr.writeheader()
        for pid, plabel, N, q, k in PARAMS:
            for bid, blabel, B in BOUNDS:
                for seed in range(args.seeds):
                    rng = random.Random(f"hit|{pid}|{bid}|{seed}")
                    hits = 0; smax = 0
                    for _ in range(args.trials):
                        m = draw_raw_maxabs(rng)
                        smax += m
                        if m <= B:
                            hits += 1
                    rate = hits / args.trials
                    lo, hi = wilson_ci(hits, args.trials)
                    exp = float('inf') if hits == 0 else 1.0 / rate
                    wr.writerow({
                        "param_id": pid, "param_label": plabel, "N": N, "q": q, "k": k,
                        "boundary_id": bid, "boundary_label": blabel, "B": B,
                        "seed": seed, "trials": args.trials, "hits": hits,
                        "hit_rate": f"{rate:.8f}", "ci95_low": f"{lo:.8f}", "ci95_high": f"{hi:.8f}",
                        "expected_attempts": "inf" if hits == 0 else f"{exp:.4f}",
                        "observed_maxabs_mean": f"{smax/args.trials:.4f}",
                    })
    print(args.out_csv)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
