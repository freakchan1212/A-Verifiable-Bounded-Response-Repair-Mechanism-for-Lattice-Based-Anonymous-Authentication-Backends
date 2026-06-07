                      
from __future__ import annotations

import argparse
import copy
import csv
import json
from pathlib import Path
from typing import Any, Dict

from fullzk.bridge import run_backend
from fullzk.generator import InstanceConfig, make_instance

def flip_hex(s: str) -> str:
    if not isinstance(s, str) or len(s) == 0:
        return "00"
    first = "0" if s[0] != "0" else "1"
    return first + s[1:]

def tamper_case(base: Dict[str, Any], name: str) -> Dict[str, Any]:
    obj = copy.deepcopy(base)
    if name == "honest":
        return obj
    if name == "tamper_delta_equality":
        obj["witness"]["delta"][0] += 1
    elif name == "tamper_bound_overflow":
        obj["witness"]["z_repaired"][0] = int(obj["params"]["B"]) + 1
    elif name == "tamper_public_t":
        obj["statement"]["public_t"][0] += 1
    elif name == "tamper_ctx_digest":
        obj["statement"]["ctx_digest"] = flip_hex(obj["statement"]["ctx_digest"])
    elif name == "tamper_repair_digest":
        obj["statement"]["repair_digest"] = flip_hex(obj["statement"]["repair_digest"])
    elif name == "tamper_refresh_tag":
        obj["statement"]["refresh_tag"] = flip_hex(obj["statement"]["refresh_tag"])
    elif name == "tamper_credential_tag":
        obj["statement"]["credential_tag"] = flip_hex(obj["statement"]["credential_tag"])
    elif name == "tamper_credential_commitment":
        obj["statement"]["credential_commitment"] = flip_hex(obj["statement"]["credential_commitment"])
    elif name == "tamper_nullifier":
        obj["statement"]["nullifier"] = flip_hex(obj["statement"]["nullifier"])
    elif name == "tamper_delta_commitment":
        obj["statement"]["delta_commitment"] = flip_hex(obj["statement"]["delta_commitment"])
    elif name == "tamper_repair_meta":
        obj["repair_meta"]["delta_l0"] += 1
    elif name == "tamper_session_scope":
        obj["public_inputs"]["scope"] = "wrong-scope"
    elif name == "tamper_delta_commit_poly":
        obj["statement"]["delta_commit_poly"][0] += 1
    elif name == "tamper_credential_commitment_poly":
        obj["statement"]["credential_commitment_poly"][0] += 1
    elif name == "tamper_nullifier_poly":
        obj["statement"]["nullifier_poly"][0] += 1
    elif name == "tamper_refresh_new_poly":
        obj["statement"]["refresh_new_poly"][0] += 1
    else:
        raise ValueError(f"unknown tamper case {name}")
    return obj

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--backend", choices=["fallback", "lazer"], default="fallback")
    ap.add_argument("--privacy", choices=["transparent", "committed"], default="committed")
    ap.add_argument("--path-mode", choices=["repair_only", "no_repair", "mixed"], default="repair_only")
    ap.add_argument("--out-dir", default="build/adversarial_v02")
    ap.add_argument("--lazer-root", default="third_party/lazer")
    args = ap.parse_args()

    cases = [
        "honest",
        "tamper_delta_equality",
        "tamper_bound_overflow",
        "tamper_public_t",
        "tamper_ctx_digest",
        "tamper_repair_digest",
        "tamper_refresh_tag",
        "tamper_credential_tag",
        "tamper_credential_commitment",
        "tamper_nullifier",
        "tamper_delta_commitment",
        "tamper_repair_meta",
        "tamper_session_scope",
        "tamper_delta_commit_poly",
        "tamper_credential_commitment_poly",
        "tamper_nullifier_poly",
        "tamper_refresh_new_poly",
    ]
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    base = make_instance(InstanceConfig(session_index=0, privacy_mode=args.privacy, path_mode=args.path_mode))
    csv_path = out_dir / "adversarial_summary.csv"
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=["case", "rc", "verify_ok", "proof_ok", "relation_ok", "repair_ok", "binding_ok", "failed_checks"])
        wr.writeheader()
        for case in cases:
            inst = tamper_case(base, case)
            case_dir = out_dir / case
            case_dir.mkdir(parents=True, exist_ok=True)
            (case_dir / "statement_witness.json").write_text(json.dumps(inst, indent=2, sort_keys=True) + "\n", encoding="utf-8")
            res = run_backend(inst, args.backend, args.lazer_root)
            (case_dir / "bridge_result.json").write_text(json.dumps(res, indent=2, sort_keys=True) + "\n", encoding="utf-8")
            rc = 0 if res.get("verify_ok") else 2
            wr.writerow({
                "case": case,
                "rc": rc,
                "verify_ok": res.get("verify_ok"),
                "proof_ok": res.get("proof_ok"),
                "relation_ok": res.get("relation_ok"),
                "repair_ok": res.get("repair_ok"),
                "binding_ok": res.get("binding_ok"),
                "failed_checks": ";".join(res.get("failed_checks", [])),
            })
    print(csv_path)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
