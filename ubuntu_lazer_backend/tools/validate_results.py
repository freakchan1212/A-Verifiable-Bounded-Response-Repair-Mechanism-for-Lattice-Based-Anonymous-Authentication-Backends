                      
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any, Dict, List

def read_csv(path: Path) -> List[Dict[str, str]]:
    if not path.exists():
        return []
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))

def fnum(x: Any, default: float = 0.0) -> float:
    try:
        return float(x)
    except Exception:
        return default

def check_file(report: List[Dict[str, Any]], path: Path, required: bool = True) -> bool:
    exists = path.exists() and path.stat().st_size > 0
    ok = exists or not required
    report.append({"check": "file_exists", "target": str(path), "required": required, "ok": ok, "exists": exists})
    return ok

def main() -> int:
    ap = argparse.ArgumentParser(description="Validate final JISA final artifact completeness and anti-footgun conditions")
    ap.add_argument("--root", default="results/final_experiments")
    ap.add_argument("--figures", default="figures")
    ap.add_argument("--require-lazer", action="store_true")
    ap.add_argument("--allow-fallback", action="store_true")
    args = ap.parse_args()
    root = Path(args.root); figs = Path(args.figures)
    report: List[Dict[str, Any]] = []
    ok_all = True

    required_files = [
        root / "system_lazer_committed_no_repair" / "batch.csv",
        root / "system_lazer_committed_repair_only" / "batch.csv",
        root / "system_lazer_committed_mixed" / "batch.csv",
        root / "derived" / "cost_breakdown_summary.csv",
        root / "derived" / "repair_metadata_leakage_summary.csv",
        root / "derived" / "final_negative_tests_summary.csv",
        root / "experiment_mapping.md",
        root / "checksums.sha256",
        figs / "figure_manifest.md",
        figs / "checksums.sha256",
    ]
    if args.require_lazer:
        required_files.append(root / "minimal_lazer_baseline.csv")
    else:
        check_file(report, root / "minimal_lazer_baseline.csv", required=False)
    for p in required_files:
        ok_all = check_file(report, p) and ok_all

                                                                       
    fig_names = [
        "fig7_1_final_boundary_hit_probability",
        "fig7_2_final_repair_vs_abort_retry_latency",
        "fig7_3_final_local_clipping_semantic_gap",
        "fig7_4_final_binding_ablation_heatmap",
        "fig7_5_final_coverage_cost_proof_bytes",
        "fig7_6_final_coverage_cost_timing",
        "fig7_7_final_leakage_delta_l0_distribution",
        "fig7_8_final_constructed_negative_tests",
    ]
    for name in fig_names:
        for sub, ext in [("data", "csv"), ("pdf", "pdf"), ("svg", "svg"), ("png", "png")]:
            ok_all = check_file(report, figs / sub / f"{name}.{ext}") and ok_all

                            
    for csv_path in [
        root / "system_lazer_committed_repair_only" / "batch.csv",
        root / "system_lazer_committed_mixed" / "batch.csv",
        root / "raw" / "repair_metadata_leakage_session.csv",
    ]:
        rows = read_csv(csv_path)
        for i, r in enumerate(rows[:1000000]):
            sr = fnum(r.get("saturated_ratio"), 0.0)
            if not (0.0 <= sr <= 1.0000001):
                report.append({"check": "saturated_ratio_range", "target": f"{csv_path}:{i}", "ok": False, "value": sr})
                ok_all = False
                break
        if rows:
            report.append({"check": "saturated_ratio_range", "target": str(csv_path), "ok": True, "rows_checked": len(rows)})

    for csv_path in [root / "system_lazer_committed_no_repair" / "batch.csv", root / "system_lazer_committed_repair_only" / "batch.csv", root / "system_lazer_committed_mixed" / "batch.csv"]:
        rows = read_csv(csv_path)
        if rows and args.require_lazer and not args.allow_fallback:
            backends = sorted(set(r.get("backend", "") for r in rows))
            ok = backends == ["lazer"]
            report.append({"check": "real_lazer_backend", "target": str(csv_path), "ok": ok, "backends": backends})
            ok_all = ok_all and ok
        if rows:
            bad_payload = [i for i, r in enumerate(rows) if fnum(r.get("payload_bytes")) + 1e-9 < fnum(r.get("proof_bytes"))]
            ok = not bad_payload
            report.append({"check": "payload_ge_proof", "target": str(csv_path), "ok": ok, "bad_rows": bad_payload[:10]})
            ok_all = ok_all and ok

                                                                                    
    cost_rows = read_csv(root / "derived" / "cost_breakdown_summary.csv")
    labels = {r.get("path_label", "") for r in cost_rows}
    needed = {"zero-offset committed control", "repair-enabled committed path", "controlled mixed committed path"}
    if args.require_lazer:
        needed.add("true minimal LaZer baseline")
    missing = sorted(needed - labels)
    ok = not missing
    report.append({"check": "cost_groups_present", "target": "derived/cost_breakdown_summary.csv", "ok": ok, "missing": missing, "labels": sorted(labels)})
    ok_all = ok_all and ok

    json_path = root / "VALIDATION_REPORT_final.json"
    md_path = root / "VALIDATION_REPORT.md"
    json_path.write_text(json.dumps({"ok": ok_all, "checks": report}, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    lines = ["# JISA final Validation Report", "", f"Overall: {'PASS' if ok_all else 'FAIL'}", "", "| Check | Target | OK | Notes |", "|---|---|---|---|"]
    for r in report:
        notes = ", ".join(f"{k}={v}" for k, v in r.items() if k not in {"check", "target", "ok"})
        lines.append(f"| {r.get('check')} | `{r.get('target')}` | {r.get('ok')} | {notes} |")
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(md_path)
    print(json_path)
    return 0 if ok_all else 2

if __name__ == "__main__":
    raise SystemExit(main())
