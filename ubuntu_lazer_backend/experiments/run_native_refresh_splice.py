from __future__ import annotations

import argparse
import csv
import copy
from pathlib import Path
from typing import Any, Dict

from experiments.run_fullrelation_sha256_smoke import add_v2_digest_fields
from fullzk.generator import InstanceConfig, digest_to_bit_polys, make_instance
from fullzk.bridge import run_backend
from fullzk.fullrelation import sha256_fullrelation_check

def flip_bit_poly(poly):
    out = list(poly)
    if not out:
        return out
    out[0] = 1 - int(out[0])
    return out

def run_case(name: str, obj: Dict[str, Any], backend: str, lazer_root: str) -> Dict[str, Any]:
    sha = sha256_fullrelation_check(obj)
    proof = run_backend(obj, backend, lazer_root)
    checks = proof.get("checks", {}) or {}
    failed = list(proof.get("failed_checks", []))
    return {
        "case": name,
        "verify_ok": bool(proof.get("verify_ok")) and bool(sha.get("sha256_fullrelation_ok")),
        "proof_ok": bool(proof.get("proof_ok")),
        "sha256_fullrelation_ok": bool(sha.get("sha256_fullrelation_ok")),
        "native_head_ok": bool(checks.get("r5_native_refresh_tag_head_bits_ok", False)) and bool(checks.get("native_refresh_tag_head_poly_ok", False)),
        "native_tail_ok": bool(checks.get("r6_native_refresh_tag_tail_bits_ok", False)) and bool(checks.get("native_refresh_tag_tail_poly_ok", False)),
        "failed_checks": ";".join(failed),
        "failed_sha256_checks": ";".join(k for k, v in (sha.get("checks", {}) or {}).items() if not v),
        "proof_bytes": int(proof.get("proof_bytes", 0) or 0),
        "prove_ms": float(proof.get("prove_ms", 0.0) or 0.0),
        "verify_ms": float(proof.get("verify_ms", 0.0) or 0.0),
        "total_ms": float(proof.get("total_ms", 0.0) or 0.0),
    }

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--backend", choices=["fallback", "lazer"], default="fallback")
    ap.add_argument("--lazer-root", default="third_party/lazer")
    ap.add_argument("--out", default="results/v2_3_native_refresh_splice.csv")
    args = ap.parse_args()

    base = add_v2_digest_fields(make_instance(InstanceConfig(privacy_mode="committed", path_mode="mixed")))
    st = base["statement"]
    assert "native_refresh_tag_head_poly" in st and "native_refresh_tag_tail_poly" in st

    cases: Dict[str, Dict[str, Any]] = {}
    cases["honest"] = copy.deepcopy(base)

    obj = copy.deepcopy(base)
    obj["statement"]["native_refresh_tag_head_poly"] = flip_bit_poly(obj["statement"]["native_refresh_tag_head_poly"])
    cases["tamper_native_head_public_bits"] = obj

    obj = copy.deepcopy(base)
    obj["statement"]["native_refresh_tag_tail_poly"] = flip_bit_poly(obj["statement"]["native_refresh_tag_tail_poly"])
    cases["tamper_native_tail_public_bits"] = obj

    obj = copy.deepcopy(base)
    obj["witness"]["slack0"] = flip_bit_poly(obj["witness"]["slack0"])
    cases["tamper_native_head_witness_bits"] = obj

    obj = copy.deepcopy(base)
    obj["witness"]["slack1"] = flip_bit_poly(obj["witness"]["slack1"])
    cases["tamper_native_tail_witness_bits"] = obj

    obj = copy.deepcopy(base)
    tampered_refresh = ("00" if obj["statement"]["refresh_tag"][:2] != "00" else "ff") + obj["statement"]["refresh_tag"][2:]
    obj["statement"]["refresh_tag"] = tampered_refresh
    cases["tamper_refresh_tag_only"] = obj

    obj = copy.deepcopy(base)
    tampered_refresh = ("00" if obj["statement"]["refresh_tag"][:2] != "00" else "ff") + obj["statement"]["refresh_tag"][2:]
    obj["statement"]["refresh_tag"] = tampered_refresh
    h, t = digest_to_bit_polys(tampered_refresh, int(obj["params"]["deg"]))
    obj["statement"]["native_refresh_tag_head_poly"] = h
    obj["statement"]["native_refresh_tag_tail_poly"] = t
    obj["witness"]["slack0"] = h
    obj["witness"]["slack1"] = t
    cases["tamper_refresh_tag_and_native_bits_consistent"] = obj

    rows = [run_case(name, obj, args.backend, args.lazer_root) for name, obj in cases.items()]
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    print(out)
    bad = [r for r in rows if (r["case"] == "honest" and not r["verify_ok"]) or (r["case"] != "honest" and r["verify_ok"])]
    if bad:
        raise SystemExit(2)

if __name__ == "__main__":
    main()
