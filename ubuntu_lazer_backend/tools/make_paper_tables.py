                      
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from statistics import mean, median
from typing import Dict, List, Any

def read_rows(path: Path) -> List[Dict[str, str]]:
    if not path.exists():
        return []
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))

def to_float(v: Any, default=0.0) -> float:
    try:
        if v is None or v == "":
            return default
        return float(v)
    except Exception:
        return default

def bool_val(v: Any) -> bool:
    return str(v).lower() in {"true", "1", "yes"}

def quantile(vals: List[float], q: float) -> float:
    if not vals:
        return 0.0
    vals = sorted(vals)
    idx = int(q * (len(vals) - 1))
    return vals[idx]

def stat(vals: List[float]) -> Dict[str, float]:
    if not vals:
        return {"mean": 0.0, "median": 0.0, "p95": 0.0, "p99": 0.0}
    return {"mean": mean(vals), "median": median(vals), "p95": quantile(vals, 0.95), "p99": quantile(vals, 0.99)}

def summarize_batch(rows: List[Dict[str, str]]) -> Dict[str, Any]:
    out: Dict[str, Any] = {"rows": len(rows)}
    for k in ["verify_ok", "proof_ok", "relation_ok", "repair_ok", "binding_ok"]:
        out[k] = f"{sum(bool_val(r.get(k)) for r in rows)}/{len(rows)}" if rows and k in rows[0] else "n/a"
    for k in ["proof_bytes", "prove_ms", "verify_ms", "total_ms", "repair_flag", "delta_l0", "meta_bytes"]:
        vals = [to_float(r.get(k)) for r in rows if k in r]
        if vals:
            out[k] = stat(vals)
    if rows and "repair_flag" in rows[0]:
        repaired = sum(int(to_float(r.get("repair_flag"))) for r in rows)
        out["repair_sessions"] = f"{repaired}/{len(rows)}"
    return out

def summarize_leakage(rows: List[Dict[str, str]]) -> Dict[str, Any]:
    out: Dict[str, Any] = {"rows": len(rows)}
    for k in ["repair_flag", "repair_count", "delta_l0", "delta_linf", "z_linf", "meta_bytes", "delta_public_len", "delta_commitment_len"]:
        vals = [to_float(r.get(k)) for r in rows if k in r]
        if vals:
            out[k] = stat(vals)
    if rows and "repair_flag" in rows[0]:
        repaired = sum(int(to_float(r.get("repair_flag"))) for r in rows)
        out["repair_sessions"] = f"{repaired}/{len(rows)}"
    if rows:
        out["privacy"] = rows[0].get("privacy", "")
        out["path_mode"] = rows[0].get("path_mode", "")
    return out

def summarize_accuracy(rows: List[Dict[str, str]]) -> Dict[str, Any]:
    if not rows:
        return {"rows": 0, "accuracy": None, "correct": 0}
    correct = sum(bool_val(r.get("correct")) for r in rows)
    return {"rows": len(rows), "correct": correct, "accuracy": correct / len(rows)}

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--artifact-dir", required=True)
    ap.add_argument("--out-md", required=True)
    ap.add_argument("--out-json", required=True)
    args = ap.parse_args()
    d = Path(args.artifact_dir)
    batch_rows = read_rows(d / "batch.csv")
    honest_rows = read_rows(d / "honest_path.csv")
    adv = read_rows(d / "adversarial_summary.csv")
    leakage_rows = read_rows(d / "leakage_metrics.csv")
    summary = {
        "batch": summarize_batch(batch_rows),
        "honest_path": summarize_batch(honest_rows),
        "adversarial": {
            "rows": len(adv),
            "accepted_cases": [r.get("case") for r in adv if bool_val(r.get("verify_ok"))],
            "rejected_cases": [r.get("case") for r in adv if not bool_val(r.get("verify_ok"))],
        },
        "leakage": summarize_leakage(leakage_rows),
        "anonymity": summarize_accuracy(read_rows(d / "anonymity_game.csv")),
        "unlinkability": summarize_accuracy(read_rows(d / "unlinkability_game.csv")),
        "zk_distinguish_sanity": summarize_accuracy(read_rows(d / "zk_distinguish_sanity.csv")),
    }
    Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out_json).write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    b = summary["batch"]
    md = []
    md.append("# Paper-ready result tables\n")
    md.append("## Table 1. Full-system acceptance and cost\n")
    md.append("| Metric | Value |\n|---|---:|\n")
    md.append(f"| Sessions | {b.get('rows',0)} |\n")
    for k in ["verify_ok", "proof_ok", "relation_ok", "repair_ok", "binding_ok", "repair_sessions"]:
        if k in b:
            md.append(f"| {k} | {b[k]} |\n")
    for k, label in [("proof_bytes", "Proof size (B)"), ("prove_ms", "Prove time (ms)"), ("verify_ms", "Verify time (ms)"), ("total_ms", "Total time (ms)")]:
        if k in b:
            v = b[k]
            md.append(f"| {label}, mean / median / p95 / p99 | {v['mean']:.3f} / {v['median']:.3f} / {v['p95']:.3f} / {v['p99']:.3f} |\n")

    md.append("\n## Table 2. Adversarial-path rejection\n")
    md.append("| Case | verify_ok | failed_checks |\n|---|---:|---|\n")
    for r in adv:
        md.append(f"| {r.get('case')} | {r.get('verify_ok')} | {r.get('failed_checks','')} |\n")

    md.append("\n## Table 3. Leakage metrics in committed-offset mode\n")
    md.append("| Metric | Mean / median / p95 |\n|---|---:|\n")
    leak = summary["leakage"]
    for k, label in [("repair_flag", "Repair flag"), ("repair_count", "Repair count"), ("delta_l0", "Delta L0"), ("delta_linf", "Delta Linf"), ("meta_bytes", "Metadata bytes"), ("delta_public_len", "Public Delta length"), ("delta_commitment_len", "Delta commitment length")]:
        if k in leak:
            v = leak[k]
            md.append(f"| {label} | {v['mean']:.3f} / {v['median']:.3f} / {v['p95']:.3f} |\n")
    if "repair_sessions" in leak:
        md.append(f"| Repair sessions in leakage sample | {leak['repair_sessions']} |\n")

    md.append("\n## Table 4. Engineering sanity checks\n")
    md.append("| Check | Rows | Accuracy | Note |\n|---|---:|---:|---|\n")
    for key, note in [("anonymity", "not a formal anonymity proof"), ("unlinkability", "not a formal unlinkability proof"), ("zk_distinguish_sanity", "not a cryptographic ZK proof")]:
        s = summary[key]
        acc = s["accuracy"]
        acc_s = "n/a" if acc is None else f"{acc:.4f}"
        md.append(f"| {key} | {s['rows']} | {acc_s} | {note} |\n")
    md.append("\n## Boundary statement\n")
    md.append("LaZer proves the core lattice relation, committed-offset repair relation, and v1.3 algebraic surrogates for Delta opening, credential commitment, nullifier/scope, and refresh transition. SHA-256 hash strings and full anonymous-credential issuance/show semantics remain bridge-level or future relation work.\n")
    Path(args.out_md).write_text("".join(md), encoding="utf-8")
    print(args.out_md)
    print(args.out_json)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
