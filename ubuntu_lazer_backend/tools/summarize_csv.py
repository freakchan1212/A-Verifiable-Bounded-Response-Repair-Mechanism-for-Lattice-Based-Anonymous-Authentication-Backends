                      
from __future__ import annotations

import argparse
import csv
import statistics
from pathlib import Path

def bool_val(x):
    return str(x).lower() in {"true", "1", "yes"}

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    args = ap.parse_args()
    rows = list(csv.DictReader(open(args.csv, encoding="utf-8")))
    print("file:", args.csv)
    print("rows:", len(rows))
    for key in ["verify_ok", "proof_ok", "relation_ok", "repair_ok", "binding_ok"]:
        if key in rows[0] if rows else False:
            vals = [bool_val(r.get(key)) for r in rows]
            print(f"{key}: {sum(vals)}/{len(vals)}")
    for key in ["proof_bytes", "prove_ms", "verify_ms", "total_ms"]:
        vals = []
        for r in rows:
            try:
                vals.append(float(r[key]))
            except Exception:
                pass
        if vals:
            vals.sort()
            p95 = vals[int(0.95 * (len(vals) - 1))]
            p99 = vals[int(0.99 * (len(vals) - 1))]
            print(f"{key}: mean={statistics.mean(vals):.3f}, median={statistics.median(vals):.3f}, p95={p95:.3f}, p99={p99:.3f}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
