from __future__ import annotations

import argparse
import copy
import csv
from pathlib import Path

from experiments.run_fullrelation_sha256_smoke import add_v2_digest_fields
from fullzk.generator import InstanceConfig, make_instance
from fullzk.fullrelation import sha256_fullrelation_check
from fullzk.bridge import run_backend

def flip_hex(s: str) -> str:
    if not s:
        return "00"
    return ("0" if s[0] != "0" else "1") + s[1:]

def apply_combo(obj, case: str):
    o = copy.deepcopy(obj)
    st = o["statement"]
    w = o["witness"]
    if case == "honest":
        return o
    if case == "ctx_refresh_combo":
        st["ctx_digest"] = flip_hex(st["ctx_digest"]); st["refresh_tag"] = flip_hex(st["refresh_tag"]); return o
    if case == "membership_nullifier_combo":
        st["v2_membership_commitment"] = flip_hex(st["v2_membership_commitment"]); st["v2_nullifier"] = flip_hex(st["v2_nullifier"]); return o
    if case == "repair_digest_delta_combo":
        st["repair_digest"] = flip_hex(st["repair_digest"]); st["delta_commitment"] = flip_hex(st["delta_commitment"]); return o
    if case == "preimage_secret_and_randomness_combo":
        w["credential_secret"] = "bad-secret"; w["delta_randomness"] = "bad-randomness"; return o
    if case == "response_and_scope_combo":
        st["public_response_digest"] = flip_hex(st["public_response_digest"]); o["public_inputs"]["scope"] = "bad-scope"; return o
    raise ValueError(case)

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--backend", choices=["fallback", "lazer"], default="fallback")
    ap.add_argument("--out-csv", default="results/v2_1_combo_tamper.csv")
    ap.add_argument("--lazer-root", default="third_party/lazer")
    args = ap.parse_args()
    base = add_v2_digest_fields(make_instance(InstanceConfig(privacy_mode="committed", path_mode="mixed")))
    cases = ["honest", "ctx_refresh_combo", "membership_nullifier_combo", "repair_digest_delta_combo", "preimage_secret_and_randomness_combo", "response_and_scope_combo"]
    rows = []
    for case in cases:
        obj = apply_combo(base, case)
        sha = sha256_fullrelation_check(obj)
        proof = run_backend(obj, args.backend, args.lazer_root)
        ok = bool(proof.get("verify_ok")) and bool(sha.get("sha256_fullrelation_ok"))
        rows.append({
            "case": case,
            "verify_ok": ok,
            "proof_ok": proof.get("proof_ok", False),
            "sha256_fullrelation_ok": sha.get("sha256_fullrelation_ok", False),
            "failed_sha256_checks": ";".join(k for k, v in sha.get("checks", {}).items() if not v),
            "failed_proof_checks": ";".join(proof.get("failed_checks", [])),
        })
    Path(args.out_csv).parent.mkdir(parents=True, exist_ok=True)
    with open(args.out_csv, "w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        wr.writeheader(); wr.writerows(rows)
    bad = [r for r in rows if (r["case"] == "honest" and not r["verify_ok"]) or (r["case"] != "honest" and r["verify_ok"])]
    print(args.out_csv)
    if bad:
        print("unexpected:", bad)
        raise SystemExit(2)

if __name__ == "__main__":
    main()
