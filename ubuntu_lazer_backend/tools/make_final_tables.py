                      
from __future__ import annotations

import argparse
import csv
import hashlib
import statistics
from pathlib import Path
from typing import Any, Dict, List

def read_csv(path: Path) -> List[Dict[str, str]]:
    if not path.exists():
        return []
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))

def write_csv(path: Path, rows: List[Dict[str, Any]], fields: List[str] | None = None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if fields is None:
        fields = []
        for r in rows:
            for k in r:
                if k not in fields:
                    fields.append(k)
    with path.open("w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=fields)
        wr.writeheader()
        wr.writerows(rows)

def fnum(x: Any) -> float:
    try:
        if x is None or str(x).strip() == "":
            return 0.0
        return float(x)
    except Exception:
        return 0.0

def bval(x: Any) -> bool:
    return str(x).strip().lower() in {"1", "true", "yes", "y"}

def pct(vals: List[float], q: float) -> float:
    vals = sorted(vals)
    if not vals:
        return 0.0
    return vals[int(q * (len(vals) - 1))]

def sha256(path: Path) -> str:
    if not path.exists() or not path.is_file():
        return "MISSING"
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def summarize_file(path: Path, label: str, kind: str) -> Dict[str, Any] | None:
    rows = read_csv(path)
    if not rows:
        return None
    out: Dict[str, Any] = {"path_label": label, "sample_kind": kind, "rows": len(rows), "source_file": str(path)}
    for metric in ["proof_bytes", "statement_bytes", "repair_metadata_bytes", "meta_bytes", "delta_commitment_len", "credential_binding_bytes", "payload_bytes", "prove_ms", "verify_ms", "total_ms", "delta_l0", "delta_linf", "saturated_ratio"]:
        vals = [fnum(r.get(metric)) for r in rows if r.get(metric, "") != ""]
        if vals:
            out[f"{metric}_mean"] = statistics.mean(vals)
            out[f"{metric}_median"] = statistics.median(vals)
            out[f"{metric}_p95"] = pct(vals, 0.95)
            out[f"{metric}_p99"] = pct(vals, 0.99)
        else:
            out[f"{metric}_mean"] = ""
            out[f"{metric}_median"] = ""
            out[f"{metric}_p95"] = ""
            out[f"{metric}_p99"] = ""
    if "verify_ok" in rows[0]:
        out["verify_rate"] = sum(bval(r.get("verify_ok")) for r in rows) / len(rows)
    return out

def make_cost_tables(root: Path) -> List[Dict[str, Any]]:
    specs = [
        ("true minimal LaZer baseline", root / "minimal_lazer_baseline.csv", "real-lazer minimal relation"),
        ("zero-offset committed control", root / "system_lazer_committed_no_repair" / "batch.csv", "same framework; no repair"),
        ("repair-enabled committed path", root / "system_lazer_committed_repair_only" / "batch.csv", "same framework; repair-only"),
        ("controlled mixed committed path", root / "system_lazer_committed_mixed" / "batch.csv", "same framework; controlled mixed"),
        ("SHA-256 fullrelation extension", root / "fullrelation_committed_mixed.csv", "extension"),
        ("output-limb splice upper-bound", root / "multi_native_splice.csv", "n=1 upper-bound plus attacks"),
    ]
    rows = []
    for label, path, kind in specs:
        s = summarize_file(path, label, kind)
        if s:
            rows.append(s)
    return rows

def make_negative_table(root: Path) -> List[Dict[str, Any]]:
    specs = [
        ("serialization", root / "serialization_fuzz_summary.csv", "accepted", "expected_accept"),
        ("state rollback", root / "state_rollback_summary.csv", "accepted", "expected_accept"),
        ("combo tamper", root / "combo_tamper_summary.csv", "verify_ok", None),
        ("profile/nonce/digest negatives", root / "profile_nonce_digest_negative.csv", "accepted", "expected_accept"),
        ("output-limb splice", root / "multi_native_splice.csv", "verify_ok", None),
    ]
    rows = []
    for category, path, accepted_col, expected_col in specs:
        data = read_csv(path)
        if not data:
            continue
        attack = rejected = honest = honest_ok = unexpected = 0
        for r in data:
            case = r.get("case", "")
            is_honest = case == "honest" or case.startswith("honest_")
            accepted = bval(r.get(accepted_col))
            if is_honest:
                honest += 1; honest_ok += int(accepted)
            else:
                attack += 1; rejected += int(not accepted)
            if expected_col and str(r.get("pass", "")).lower() in {"false", "0"}:
                unexpected += 1
            elif not expected_col:
                if is_honest and not accepted: unexpected += 1
                if (not is_honest) and accepted: unexpected += 1
        rows.append({
            "category": category,
            "honest_samples": honest,
            "honest_accept_rate": honest_ok / honest if honest else "",
            "attack_samples": attack,
            "rejected_attack_samples": rejected,
            "attack_rejection_rate": rejected / attack if attack else "",
            "unexpected_rows": unexpected,
            "source_file": str(path),
        })
    return rows

def make_artifact_mapping(root: Path, out_dir: Path) -> List[Dict[str, Any]]:
    rows = [
        ("Boundary hit probability", "raw/boundary_hit_probability.csv", "experiments/run_boundary_probability.py", "Figure 7-1; Table 7-2"),
        ("Repair vs abort/retry", "raw/abort_retry_session.csv; derived/abort_retry_summary.csv; derived/abort_retry_seed_level.csv", "experiments/run_abort_retry_sweep.py; tools/summarize_seed_level.py", "Figure 7-2; Table 7-3"),
        ("Local clipping semantic gap", "raw/local_clipping_baseline_session.csv; derived/local_clipping_baseline_summary.csv", "experiments/run_local_clipping_baseline.py", "Figure 7-3; Table 7-4"),
        ("Context/refresh binding ablation", "raw/binding_ablation_session.csv; derived/binding_ablation_summary.csv", "experiments/run_binding_ablation.py", "Figure 7-4; Table 7-5"),
        ("Repair metadata leakage", "raw/repair_metadata_leakage_session.csv; derived/repair_metadata_leakage_summary.csv; derived/repair_metadata_leakage_seed_level.csv", "experiments/run_repair_metadata_leakage.py; tools/summarize_seed_level.py", "Figure 7-7; leakage table"),
        ("True minimal LaZer baseline", "minimal_lazer_baseline.csv; minimal_lazer_baseline_seed_level.csv", "experiments/run_minimal_lazer_baseline.py", "Table 7-x minimal baseline"),
        ("Zero-offset committed control", "system_lazer_committed_no_repair/batch.csv; system_lazer_committed_no_repair/batch_seed_level.csv", "experiments/run_batch.py", "Table 7-x cost breakdown"),
        ("Repair-enabled committed path", "system_lazer_committed_repair_only/batch.csv; system_lazer_committed_repair_only/batch_seed_level.csv", "experiments/run_batch.py", "Table 7-x cost breakdown"),
        ("Controlled mixed committed path", "system_lazer_committed_mixed/batch.csv; system_lazer_committed_mixed/batch_seed_level.csv", "experiments/run_batch.py", "Figure 7-5/7-6"),
        ("SHA-256 fullrelation extension", "fullrelation_committed_mixed.csv; fullrelation_committed_mixed_seed_level.csv", "experiments/run_fullrelation_batch.py", "Figure 7-5/7-6; Table 7-7"),
        ("Output-limb splice", "multi_native_splice.csv", "experiments/run_multi_digest_splice.py", "Figure 7-8; extension table"),
        ("Workload profiles", "raw/workload_profiles.csv; raw/workload_profiles_final_summary.csv", "experiments/run_workload_profiles.py", "Workload profile table"),
        ("Negative tests", "serialization_fuzz_summary.csv; state_rollback_summary.csv; combo_tamper_summary.csv; profile_nonce_digest_negative.csv", "experiments/run_serialization_fuzz.py; experiments/run_state_rollback.py; experiments/run_combo_tamper.py; experiments/run_profile_nonce_digest_negative.py", "Figure 7-8; robustness table"),
        ("Memory footprint", "derived/memory_footprint.csv", "tools/collect_memory_footprint.py", "Supplementary memory table/figure"),
        ("final figures", "figures/data/*.csv", "tools/make_final_figures.py", "Figures 7-1 to 7-8 plus supplementary"),
    ]
    out = []
    for result, raw, script, fig in rows:
        shas = []
        for item in raw.split(";"):
            item = item.strip()
            if "*" in item:
                shas.append(f"{item}=GLOB")
            else:
                shas.append(f"{item}={sha256(root / item)}")
        out.append({"result": result, "raw_file": raw, "script": script, "figure_or_table": fig, "sha256": "; ".join(shas)})
    return out

def main() -> int:
    ap = argparse.ArgumentParser(description="Create JISA final paper tables from completed experiment artifact")
    ap.add_argument("--root", default="results/final_experiments")
    ap.add_argument("--out-dir", default="results/final_experiments/derived")
    args = ap.parse_args()
    root = Path(args.root)
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)

    cost = make_cost_tables(root)
    write_csv(out / "cost_breakdown_summary.csv", cost)

    negative = make_negative_table(root)
    write_csv(out / "final_negative_tests_summary.csv", negative)

    artifact = make_artifact_mapping(root, out)
    write_csv(root / "artifact_mapping_final.csv", artifact)
    md_lines = ["# JISA final Artifact Mapping", "", "| Result | Raw file | Script | Figure/Table | SHA-256 |", "|---|---|---|---|---|"]
    for r in artifact:
        md_lines.append(f"| {r['result']} | `{r['raw_file']}` | `{r['script']}` | {r['figure_or_table']} | `{r['sha256']}` |")
    (root / "experiment_mapping.md").write_text("\n".join(md_lines) + "\n", encoding="utf-8")

    print(out / "cost_breakdown_summary.csv")
    print(out / "final_negative_tests_summary.csv")
    print(root / "experiment_mapping.md")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
