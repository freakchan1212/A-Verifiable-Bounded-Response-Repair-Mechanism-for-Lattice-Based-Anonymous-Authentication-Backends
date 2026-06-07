from __future__ import annotations

import argparse
import csv
import copy
from pathlib import Path
from typing import Any, Dict, List

from experiments.run_fullrelation_sha256_smoke import add_v2_digest_fields
from fullzk.generator import (
    InstanceConfig,
    NATIVE_DIGEST_RELATIONS,
    digest_to_bit_polys,
    make_instance,
    native_head_field,
    native_tail_field,
    native_head_witness,
    native_tail_witness,
)
from fullzk.bridge import run_backend
from fullzk.fullrelation import sha256_fullrelation_check

def flip_bit_poly(poly: List[int]) -> List[int]:
    out = list(poly)
    if not out:
        return out
    out[0] = 1 - int(out[0])
    return out

def tamper_hex_digest(x: str) -> str:
    return ("00" if x[:2] != "00" else "ff") + x[2:]

def native_ok(checks: Dict[str, Any], name: str) -> tuple[bool, bool]:
    head_key = f"native_{name}_head_poly_ok"
    tail_key = f"native_{name}_tail_poly_ok"
    row_head = next((k for k in checks if k.startswith("r") and k.endswith(f"native_{name}_head_bits_ok")), "")
    row_tail = next((k for k in checks if k.startswith("r") and k.endswith(f"native_{name}_tail_bits_ok")), "")
    return bool(checks.get(head_key, False) and checks.get(row_head, False)), bool(checks.get(tail_key, False) and checks.get(row_tail, False))

def run_case(name: str, obj: Dict[str, Any], backend: str, lazer_root: str, relation: str = "") -> Dict[str, Any]:
    sha = sha256_fullrelation_check(obj)
    proof = run_backend(obj, backend, lazer_root)
    checks = proof.get("checks", {}) or {}
    failed = list(proof.get("failed_checks", []))
    if relation:
        h_ok, t_ok = native_ok(checks, relation)
    else:
        h_ok = all(native_ok(checks, r)[0] for r in NATIVE_DIGEST_RELATIONS)
        t_ok = all(native_ok(checks, r)[1] for r in NATIVE_DIGEST_RELATIONS)
    return {
        "case": name,
        "relation": relation or "all",
        "verify_ok": bool(proof.get("verify_ok")) and bool(sha.get("sha256_fullrelation_ok")),
        "proof_ok": bool(proof.get("proof_ok")),
        "sha256_fullrelation_ok": bool(sha.get("sha256_fullrelation_ok")),
        "native_head_ok": bool(h_ok),
        "native_tail_ok": bool(t_ok),
        "failed_checks": ";".join(failed),
        "failed_sha256_checks": ";".join(k for k, v in (sha.get("checks", {}) or {}).items() if not v),
        "proof_bytes": int(proof.get("proof_bytes", 0) or 0),
        "prove_ms": float(proof.get("prove_ms", 0.0) or 0.0),
        "verify_ms": float(proof.get("verify_ms", 0.0) or 0.0),
        "total_ms": float(proof.get("total_ms", 0.0) or 0.0),
    }

def build_cases(base: Dict[str, Any], exhaustive: bool = False) -> List[tuple[str, str, Dict[str, Any]]]:
    rows: List[tuple[str, str, Dict[str, Any]]] = [("honest", "", copy.deepcopy(base))]
    rels = list(NATIVE_DIGEST_RELATIONS)
    for rel in rels:
        hf, tf = native_head_field(rel), native_tail_field(rel)
        hw, tw = native_head_witness(rel), native_tail_witness(rel)

        obj = copy.deepcopy(base)
        obj["statement"][hf] = flip_bit_poly(obj["statement"][hf])
        rows.append((f"tamper_{rel}_head_public_bits", rel, obj))

        obj = copy.deepcopy(base)
        obj["statement"][tf] = flip_bit_poly(obj["statement"][tf])
        rows.append((f"tamper_{rel}_tail_public_bits", rel, obj))

        obj = copy.deepcopy(base)
        obj["witness"][hw] = flip_bit_poly(obj["witness"][hw])
        rows.append((f"tamper_{rel}_head_witness_bits", rel, obj))

        obj = copy.deepcopy(base)
        obj["witness"][tw] = flip_bit_poly(obj["witness"][tw])
        rows.append((f"tamper_{rel}_tail_witness_bits", rel, obj))

        obj = copy.deepcopy(base)
        obj["statement"][rel] = tamper_hex_digest(obj["statement"][rel])
        rows.append((f"tamper_{rel}_digest_only", rel, obj))

        if exhaustive:
            obj = copy.deepcopy(base)
            new_digest = tamper_hex_digest(obj["statement"][rel])
            obj["statement"][rel] = new_digest
            h, t = digest_to_bit_polys(new_digest, int(obj["params"]["deg"]))
            obj["statement"][hf] = h
            obj["statement"][tf] = t
            obj["witness"][hw] = h
            obj["witness"][tw] = t
            rows.append((f"tamper_{rel}_digest_and_native_bits_consistent", rel, obj))
    return rows

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--backend", choices=["fallback", "lazer"], default="fallback")
    ap.add_argument("--lazer-root", default="third_party/lazer")
    ap.add_argument("--out", default="results/v2_4_multi_native_splice.csv")
    ap.add_argument("--exhaustive", action="store_true", help="also test digest+native-consistent tampering for every relation")
    args = ap.parse_args()

    base = add_v2_digest_fields(make_instance(InstanceConfig(privacy_mode="committed", path_mode="mixed")))
    cases = build_cases(base, exhaustive=args.exhaustive)
    result_rows = [run_case(name, obj, args.backend, args.lazer_root, rel) for name, rel, obj in cases]

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(result_rows[0].keys()))
        w.writeheader(); w.writerows(result_rows)
    print(out)
    bad = [r for r in result_rows if (r["case"] == "honest" and not r["verify_ok"]) or (r["case"] != "honest" and r["verify_ok"])]
    if bad:
        print("unexpected accept/reject rows:", bad[:3])
        raise SystemExit(2)

if __name__ == "__main__":
    main()
