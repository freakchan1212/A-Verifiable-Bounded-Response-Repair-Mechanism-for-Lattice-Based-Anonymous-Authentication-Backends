from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Dict, List

from fullzk.circuits.sha256_native_lowering import lower_sha256_ir_relation, summarize_lowerings

def _load_or_export_ir(path: Path) -> Dict[str, Any]:
    if path.exists():
        return json.loads(path.read_text(encoding="utf-8"))
    raise FileNotFoundError(f"missing IR file: {path}. Run tools/export_sha256_constraint_ir.py first.")

def _write_scaffold(out_dir: Path, rel: Dict[str, Any]) -> Path:
    name = rel["relation_name"]
    path = out_dir / f"native_sha256_{name}_scaffold.py"
    text = f'''"""Auto-generated v2.2 native SHA-256 LaZer scaffold for {name}.

This scaffold is intentionally conservative: it describes the lowered constraint
plan that should be mapped to LaZer gadgets. It is not imported by the prover yet.
The corresponding machine-readable plan is native_sha256_{name}_plan.json.
"""

RELATION_NAME = {name!r}
BLOCKS = {rel["blocks"]}
ROUNDS = {rel["rounds"]}
EST_BOOLEAN_CONSTRAINTS = {rel["estimated_boolean_constraints"]}
EST_WORD_CONSTRAINTS = {rel["estimated_word_constraints"]}
LOWERED_NATIVE_PLAN_CONSTRAINTS = {rel["lowered_constraint_count"]}

def register_constraints(builder):
    """Future hook: map the native plan into LaZer relation builder gadgets."""
    raise NotImplementedError(
        "v2.2 emits a native constraint plan; v2.3 must map it to concrete LaZer gadgets"
    )
'''
    path.write_text(text, encoding="utf-8")
    return path

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ir-json", default="results/v2_1_sha256_constraint_ir.json")
    ap.add_argument("--relations", default="v2_refresh_tag,v2_nullifier", help="comma-separated relation names or 'all'")
    ap.add_argument("--out-dir", default="results/v2_2_native_sha256_lowering")
    args = ap.parse_args()

    ir_doc = _load_or_export_ir(Path(args.ir_json))
    all_rel = {r["relation_name"]: r for r in ir_doc.get("relations", [])}
    selected = list(all_rel) if args.relations == "all" else [x.strip() for x in args.relations.split(",") if x.strip()]
    missing = [x for x in selected if x not in all_rel]
    if missing:
        raise SystemExit(f"missing relations in IR: {missing}; available={sorted(all_rel)}")

    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    results = [lower_sha256_ir_relation(all_rel[name]) for name in selected]
    summary = summarize_lowerings(results)
    summary["source_ir"] = str(Path(args.ir_json))
    summary["selected_relations"] = selected

    for res in results:
        d = res.to_dict()
        (out / f"native_sha256_{res.relation_name}_plan.json").write_text(json.dumps(d, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        _write_scaffold(out, d)

    (out / "native_sha256_lowering_summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    lines = [
        "# v2.2 native SHA-256-to-LaZer lowering prototype",
        "",
        "This is a native constraint-plan prototype. It lowers SHA-256 IR into LaZer-friendly bit/word constraints, but it is not yet spliced into `auth_repair_params.h`.",
        "",
        "| Relation | Blocks | Rounds | Est. boolean constraints | Est. word constraints | Lowered native-plan constraints |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for res in results:
        lines.append(f"| {res.relation_name} | {res.blocks} | {res.rounds} | {res.estimated_boolean_constraints} | {res.estimated_word_constraints} | {res.lowered_constraint_count} |")
    lines += [
        f"| **Total** | **{summary['total_blocks']}** | **{summary['total_rounds']}** | **{summary['total_estimated_boolean_constraints']}** | **{summary['total_estimated_word_constraints']}** | **{summary['total_native_plan_constraints']}** |",
        "",
        "## Boundary",
        "- Achieved in v2.2: deterministic IR-to-native-plan lowering for selected SHA-256 relations.",
        "- Not yet achieved: concrete LaZer gadget mapping and inclusion in proof generation.",
    ]
    (out / "native_sha256_lowering_summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(out / "native_sha256_lowering_summary.md")
    print(out / "native_sha256_lowering_summary.json")

if __name__ == "__main__":
    main()
