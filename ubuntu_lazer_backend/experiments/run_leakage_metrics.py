                      
from __future__ import annotations

import argparse
import csv
from pathlib import Path
from statistics import mean

from fullzk.protocol import SystemParams, Issuer, DeviceProver

def main() -> int:
    ap = argparse.ArgumentParser(description="Leakage metrics for committed/transparent and repair/no-repair/mixed modes")
    ap.add_argument("--sessions", type=int, default=100)
    ap.add_argument("--seeds", type=int, default=1)
    ap.add_argument("--privacy", choices=["committed", "transparent"], default="committed")
    ap.add_argument("--path-mode", choices=["repair_only", "no_repair", "mixed"], default="mixed")
    ap.add_argument("--out-csv", default="results/system_v1_leakage_metrics.csv")
    ap.add_argument("--summary", default="results/system_v1_leakage_summary.txt")
    args = ap.parse_args()

    Path(args.out_csv).parent.mkdir(parents=True, exist_ok=True)
    params = SystemParams(privacy_mode=args.privacy, nullifier_mode="unlinkable-demo", path_mode=args.path_mode)
    rows = []
    with open(args.out_csv, "w", newline="", encoding="utf-8") as f:
        fields = ["seed", "session", "privacy", "path_mode", "B", "repair_flag", "repair_count", "delta_l0", "delta_linf", "z_linf", "meta_bytes", "delta_public_len", "delta_commitment_len", "saturated_pos", "saturated_neg", "saturated_total", "saturated_ratio"]
        wr = csv.DictWriter(f, fieldnames=fields); wr.writeheader()
        for seed in range(args.seeds):
            cred = Issuer(params).issue(f"device-A-seed-{seed}")
            prover = DeviceProver(params, cred)
            for i in range(args.sessions):
                inst = prover.authenticate(i).statement_witness
                meta = inst.get("repair_meta", {})
                st = inst.get("statement", {})
                z_public = st.get("z_public", []) or []
                B = int(inst.get("params", {}).get("B", 0) or 0)
                if B == 0:
                    saturated_pos = sum(1 for x in z_public if int(x) == 0)
                    saturated_neg = 0
                else:
                    saturated_pos = sum(1 for x in z_public if int(x) == B)
                    saturated_neg = sum(1 for x in z_public if int(x) == -B)
                saturated_total = saturated_pos + saturated_neg
                row = {
                    "seed": seed,
                    "session": i,
                    "privacy": args.privacy,
                    "path_mode": args.path_mode,
                    "B": inst.get("params", {}).get("B"),
                    "repair_flag": meta.get("repair_flag", 0),
                    "repair_count": meta.get("repair_count", 0),
                    "delta_l0": meta.get("delta_l0", 0),
                    "delta_linf": meta.get("delta_linf", 0),
                    "z_linf": meta.get("z_linf", 0),
                    "meta_bytes": meta.get("meta_bytes", 0),
                    "delta_public_len": len(st.get("delta_public", [])),
                    "delta_commitment_len": len(st.get("delta_commitment", "")),
                    "saturated_pos": saturated_pos,
                    "saturated_neg": saturated_neg,
                    "saturated_total": saturated_total,
                    "saturated_ratio": saturated_total / len(z_public) if z_public else 0.0,
                }
                rows.append(row); wr.writerow(row)
    lines = [f"file: {args.out_csv}", f"rows: {len(rows)}", f"privacy: {args.privacy}", f"path_mode: {args.path_mode}"]
    for k in ["repair_flag", "repair_count", "delta_l0", "delta_linf", "z_linf", "meta_bytes", "delta_public_len", "delta_commitment_len", "saturated_total", "saturated_ratio"]:
        vals = [float(r[k]) for r in rows]
        lines.append(f"{k}: mean={mean(vals):.3f}, min={min(vals):.3f}, max={max(vals):.3f}")
                                         
    repaired = sum(1 for r in rows if int(r["repair_flag"]) == 1)
    lines.append(f"repair_sessions: {repaired}/{len(rows)}")
    Path(args.summary).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
