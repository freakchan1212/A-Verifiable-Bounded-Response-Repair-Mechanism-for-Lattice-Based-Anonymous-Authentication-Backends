from __future__ import annotations

import argparse
import json
from pathlib import Path

from experiments.run_fullrelation_sha256_smoke import add_v2_digest_fields
from fullzk.generator import InstanceConfig, make_instance
from fullzk.fullrelation import _preimages
from fullzk.circuits.sha256_bits import sha256_hexdigest
from fullzk.circuits.sha256_ir import build_sha256_constraint_ir

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-json", default="results/v2_1_sha256_constraint_ir.json")
    ap.add_argument("--out-md", default="results/v2_1_sha256_constraint_ir.md")
    args = ap.parse_args()
    obj = add_v2_digest_fields(make_instance(InstanceConfig(privacy_mode="committed", path_mode="mixed")))
    pre = _preimages(obj)
    st = obj["statement"]
    digest_fields = {
        "commitment": "commitment",
        "challenge": "challenge",
        "ctx_digest": "ctx_digest",
        "repair_digest": "repair_digest",
        "public_response_digest": "public_response_digest",
        "delta_commitment": "delta_commitment",
        "v2_membership_commitment": "v2_membership_commitment",
        "v2_membership_tag": "v2_membership_tag",
        "v2_nullifier": "v2_nullifier",
        "v2_refresh_tag": "v2_refresh_tag",
    }
    irs = []
    for rel, field in digest_fields.items():
        expected = st.get(field) or sha256_hexdigest(pre[rel])
        irs.append(build_sha256_constraint_ir(rel, pre[rel], expected).to_dict())
    summary = {
        "version": "v2.1-sha256-authrelation-hardening",
        "status": "constraint IR export; not native LaZer constraints",
        "relations": irs,
        "totals": {
            "rounds": sum(x["rounds"] for x in irs),
            "estimated_boolean_constraints": sum(x["estimated_boolean_constraints"] for x in irs),
            "estimated_word_constraints": sum(x["estimated_word_constraints"] for x in irs),
            "operations": sum(len(x["operations"]) for x in irs),
        }
    }
    out_json = Path(args.out_json); out_md = Path(args.out_md)
    out_json.parent.mkdir(parents=True, exist_ok=True)
    out_json.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    lines = ["# SHA-256 constraint IR export", "", "This file is a compiler-facing intermediate representation. It is not yet a native LaZer constraint file.", "", "| Relation | Blocks | Rounds | Est. boolean constraints | Est. word constraints |", "|---|---:|---:|---:|---:|"]
    for x in irs:
        lines.append(f"| {x['relation_name']} | {x['blocks']} | {x['rounds']} | {x['estimated_boolean_constraints']} | {x['estimated_word_constraints']} |")
    t = summary["totals"]
    lines.append(f"| **Total** |  | **{t['rounds']}** | **{t['estimated_boolean_constraints']}** | **{t['estimated_word_constraints']}** |")
    out_md.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(out_json)
    print(out_md)

if __name__ == "__main__":
    main()
