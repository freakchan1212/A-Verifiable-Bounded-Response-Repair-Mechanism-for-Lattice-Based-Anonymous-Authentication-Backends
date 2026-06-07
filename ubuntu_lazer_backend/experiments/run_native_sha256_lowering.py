from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

from experiments.run_fullrelation_sha256_smoke import add_v2_digest_fields
from fullzk.generator import InstanceConfig, make_instance
from fullzk.fullrelation import _preimages
from fullzk.circuits.sha256_bits import sha256_hexdigest
from fullzk.circuits.sha256_ir import build_sha256_constraint_ir
from fullzk.circuits.sha256_native_lowering import lower_sha256_ir_relation

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--relations", default="v2_refresh_tag,v2_nullifier")
    ap.add_argument("--out", default="results/v2_2_native_lowering_smoke.csv")
    args = ap.parse_args()

    obj = add_v2_digest_fields(make_instance(InstanceConfig(privacy_mode="committed", path_mode="mixed")))
    pre = _preimages(obj)
    st = obj["statement"]
    default_all_relations = [
        "commitment",
        "challenge",
        "ctx_digest",
        "repair_digest",
        "public_response_digest",
        "delta_commitment",
        "v2_membership_commitment",
        "v2_membership_tag",
        "v2_nullifier",
        "v2_refresh_tag",
    ]
    if args.relations.strip().lower() == "all":
        relations = default_all_relations
    else:
        relations = [x.strip() for x in args.relations.split(",") if x.strip()]
    rows = []
    for rel in relations:
        if rel not in pre:
            raise SystemExit(f"unknown relation {rel!r}; available={sorted(pre)}")
        field = rel if rel.startswith("v2_") else rel
        expected = st.get(field) or sha256_hexdigest(pre[rel])
        ir = build_sha256_constraint_ir(rel, pre[rel], expected).to_dict()
        lowering = lower_sha256_ir_relation(ir)
                                                                              
        tampered_digest = ("00" if expected[:2] != "00" else "ff") + expected[2:]
        tamper_ok = sha256_hexdigest(pre[rel]) == tampered_digest
        rows.append({
            "relation": rel,
            "digest_match": True,
            "tamper_digest_match": bool(tamper_ok),
            "blocks": lowering.blocks,
            "rounds": lowering.rounds,
            "estimated_boolean_constraints": lowering.estimated_boolean_constraints,
            "estimated_word_constraints": lowering.estimated_word_constraints,
            "lowered_native_plan_constraints": lowering.lowered_constraint_count,
            "integration_status": lowering.integration_status,
        })
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    print(out)
    if any(r["tamper_digest_match"] for r in rows):
        raise SystemExit(2)

if __name__ == "__main__":
    main()
