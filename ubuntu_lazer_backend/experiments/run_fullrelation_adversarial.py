from __future__ import annotations

import argparse
import csv
import copy
from pathlib import Path

from experiments.run_fullrelation_sha256_smoke import add_v2_digest_fields
from fullzk.generator import InstanceConfig, make_instance
from fullzk.fullrelation import sha256_fullrelation_check
from fullzk.bridge import run_backend

def flip_hex(s: str) -> str:
    if not s:
        return "00"
    c = "0" if s[0] != "0" else "1"
    return c + s[1:]

def tamper(obj, case: str):
    o = copy.deepcopy(obj)
    st = o["statement"]
    if case == "honest":
        return o
    if case in st:
        st[case] = flip_hex(st[case])
        return o
    if case == "delta_equality":
        o["witness"]["delta"][0] += 1
        return o
    if case == "member_secret_preimage":
        o["witness"]["credential_secret"] = "bad-secret"
        return o
    if case == "nullifier_randomness_preimage":
        o["witness"]["delta_randomness"] = "bad-randomness"
        return o
    raise ValueError(case)

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--backend", choices=["fallback", "lazer"], default="fallback")
    ap.add_argument("--out-csv", default="results/v2_fullrelation_adversarial.csv")
    ap.add_argument("--lazer-root", default="third_party/lazer")
    args = ap.parse_args()
    base = add_v2_digest_fields(make_instance(InstanceConfig(privacy_mode="committed", path_mode="mixed")))
    cases = [
        "honest",
        "ctx_digest",
        "repair_digest",
        "public_response_digest",
        "delta_commitment",
        "credential_commitment",
        "credential_tag",
        "nullifier",
        "refresh_tag",
        "v2_membership_commitment",
        "v2_membership_tag",
        "v2_nullifier",
        "v2_refresh_tag",
        "delta_equality",
        "member_secret_preimage",
        "nullifier_randomness_preimage",
    ]
    rows = []
    for case in cases:
        obj = tamper(base, case)
        sha = sha256_fullrelation_check(obj)
        proof = run_backend(obj, args.backend, args.lazer_root)
        ok = bool(proof.get("verify_ok")) and bool(sha.get("sha256_fullrelation_ok"))
        failed_sha = [k for k, v in sha.get("checks", {}).items() if not v]
        failed_proof = proof.get("failed_checks", [])
        rows.append({
            "case": case,
            "verify_ok": ok,
            "proof_ok": proof.get("proof_ok", False),
            "sha256_fullrelation_ok": sha.get("sha256_fullrelation_ok", False),
            "failed_sha256_checks": ";".join(failed_sha),
            "failed_proof_checks": ";".join(failed_proof),
        })
    Path(args.out_csv).parent.mkdir(parents=True, exist_ok=True)
    with open(args.out_csv, "w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        wr.writeheader(); wr.writerows(rows)
    print(args.out_csv)
    bad = [r for r in rows if (r["case"] == "honest" and str(r["verify_ok"]).lower() != "true") or (r["case"] != "honest" and str(r["verify_ok"]).lower() == "true")]
    if bad:
        print("unexpected:", bad)
        raise SystemExit(2)

if __name__ == "__main__":
    main()
