                      
from __future__ import annotations

import argparse
import csv
import math
import statistics
from pathlib import Path

def fnum(x):
    try:
        if str(x).strip() in {"", "None", "nan"}:
            return None
        return float(x)
    except Exception:
        return None

def mean_ci(vals):
    vals = [v for v in vals if v is not None]
    if not vals:
        return "", "", "", ""
    m = statistics.mean(vals)
    if len(vals) == 1:
        return m, 0.0, m, m
    sd = statistics.stdev(vals)
    half = 1.96 * sd / math.sqrt(len(vals))
    return m, sd, m - half, m + half

def main() -> int:
    ap = argparse.ArgumentParser(description="Seed-level mean/std/95% CI summarizer for JISA review rescue experiments")
    ap.add_argument("--csv", required=True, help="input CSV containing a seed column")
    ap.add_argument("--out", required=True)
    ap.add_argument("--group-cols", default="", help="comma-separated grouping columns; omit for one global group")
    ap.add_argument("--metrics", required=True, help="comma-separated numeric metrics to summarize")
    args = ap.parse_args()

    rows = list(csv.DictReader(open(args.csv, encoding="utf-8")))
    group_cols = [x.strip() for x in args.group_cols.split(",") if x.strip()]
    metrics = [x.strip() for x in args.metrics.split(",") if x.strip()]
    if not rows:
        raise SystemExit(f"empty CSV: {args.csv}")
    if "seed" not in rows[0]:
        raise SystemExit(f"CSV has no seed column: {args.csv}")

    groups = {}
    for r in rows:
        key = tuple(r.get(c, "") for c in group_cols) if group_cols else ("all",)
        groups.setdefault(key, []).append(r)

    out_fields = group_cols + ["metric", "seed_count", "session_count", "mean_of_seed_means", "std_of_seed_means", "ci95_low", "ci95_high", "min_seed_mean", "max_seed_mean"]
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    with open(args.out, "w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=out_fields)
        wr.writeheader()
        for key, rs in sorted(groups.items()):
            by_seed = {}
            for r in rs:
                by_seed.setdefault(r["seed"], []).append(r)
            for metric in metrics:
                seed_means = []
                for s, srows in by_seed.items():
                    vals = [fnum(r.get(metric)) for r in srows]
                    vals = [v for v in vals if v is not None]
                    if vals:
                        seed_means.append(statistics.mean(vals))
                m, sd, lo, hi = mean_ci(seed_means)
                base = {c: key[i] for i, c in enumerate(group_cols)}
                base.update({
                    "metric": metric,
                    "seed_count": len(seed_means),
                    "session_count": len(rs),
                    "mean_of_seed_means": f"{m:.8f}" if m != "" else "",
                    "std_of_seed_means": f"{sd:.8f}" if sd != "" else "",
                    "ci95_low": f"{lo:.8f}" if lo != "" else "",
                    "ci95_high": f"{hi:.8f}" if hi != "" else "",
                    "min_seed_mean": f"{min(seed_means):.8f}" if seed_means else "",
                    "max_seed_mean": f"{max(seed_means):.8f}" if seed_means else "",
                })
                wr.writerow(base)
    print(args.out)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
