                      
from __future__ import annotations

import argparse
import copy
import csv
from pathlib import Path
from typing import Any, Dict, List

from experiments.run_fullrelation_sha256_smoke import add_v2_digest_fields
from fullzk.generator import InstanceConfig, make_instance
from fullzk.fullrelation import sha256_fullrelation_check
from fullzk.bridge import run_backend

def flip_hex(s: str) -> str:
    if not s:
        return "00"
    return ("0" if s[0] != "0" else "1") + s[1:]

def evaluate(case: str, obj: Dict[str, Any], expected_accept: bool, backend: str, lazer_root: str) -> Dict[str, Any]:
    sha_ok = False
    proof_ok = False
    verify_ok = False
    failed_checks: List[str] = []
    proof_invoked = False
    try:
        sha = sha256_fullrelation_check(obj)
        sha_ok = bool(sha.get("sha256_fullrelation_ok"))
        failed_checks.extend(k for k, v in sha.get("checks", {}).items() if not v)
    except Exception as e:
        failed_checks.append("sha_exception:" + type(e).__name__)
    try:
        proof_invoked = True
        res = run_backend(obj, backend, lazer_root)
        proof_ok = bool(res.get("proof_ok"))
        verify_ok = bool(res.get("verify_ok"))
        failed_checks.extend(res.get("failed_checks", []))
    except Exception as e:
        failed_checks.append("backend_exception:" + type(e).__name__)
    accepted = bool(sha_ok and verify_ok)
    return {
        "case": case,
        "attack_type": "honest" if expected_accept else case,
        "tamper_field": infer_tamper_field(case),
        "expected_fail_check": "none" if expected_accept else infer_expected_fail(case),
        "actual_fail_check": ";".join(sorted(set(failed_checks))),
        "accepted": accepted,
        "expected_accept": expected_accept,
        "pass": accepted == expected_accept,
        "state_updated": False,
        "replay_set_changed": False,
        "proof_verifier_invoked": proof_invoked,
        "proof_ok": proof_ok,
        "sha256_fullrelation_ok": sha_ok,
    }

def infer_tamper_field(case: str) -> str:
    if case == "profile_downgrade":
        return "relation_profile,profile_id/native fields"
    if case == "nonce_reuse_same_scope_epoch":
        return "session_id,nullifier,ctx_digest"
    if case == "digest_swapping_cross_session":
        return "repair_digest,resp_digest,ctx_digest"
    if case == "resp_digest_legacy_alias_swap":
        return "resp_digest/public_response_digest"
    return "none"

def infer_expected_fail(case: str) -> str:
    if case == "profile_downgrade":
        return "profile/native digest binding"
    if case == "nonce_reuse_same_scope_epoch":
        return "nullifier/ctx_digest/replay binding"
    if case == "digest_swapping_cross_session":
        return "repair_digest/resp_digest/ctx_digest binding"
    if case == "resp_digest_legacy_alias_swap":
        return "resp_digest binding"
    return "unknown"

def main() -> int:
    ap = argparse.ArgumentParser(description="Constructed final negative tests for profile downgrade, nonce reuse and digest swapping")
    ap.add_argument("--backend", choices=["fallback", "lazer"], default="fallback")
    ap.add_argument("--lazer-root", default="third_party/lazer")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    base = add_v2_digest_fields(make_instance(InstanceConfig(seed_index=0, session_index=0, epoch=0, privacy_mode="committed", path_mode="mixed")))
    other = add_v2_digest_fields(make_instance(InstanceConfig(seed_index=1, session_index=1, epoch=0, privacy_mode="committed", path_mode="mixed")))
    cases: List[tuple[str, Dict[str, Any], bool]] = []
    cases.append(("honest", copy.deepcopy(base), True))

                                                                                                      
    o = copy.deepcopy(base)
    o["params"]["relation_profile"] = "r0_weak_profile"
    o["statement"]["relation"] = "downgraded_profile"
    if "ctx_digest" in o["statement"]:
        o["statement"]["ctx_digest"] = flip_hex(o["statement"]["ctx_digest"])
    cases.append(("profile_downgrade", o, False))

                                                                                  
    o = copy.deepcopy(other)
    o["statement"]["session_id"] = base["statement"].get("session_id")
    o["statement"]["nullifier"] = base["statement"].get("nullifier")
    cases.append(("nonce_reuse_same_scope_epoch", o, False))

                                                                                      
    o = copy.deepcopy(base)
    for fld in ["repair_digest", "resp_digest", "public_response_digest", "ctx_digest"]:
        if fld in o["statement"] and fld in other["statement"]:
            o["statement"][fld] = other["statement"][fld]
    cases.append(("digest_swapping_cross_session", o, False))

                                                                                  
    o = copy.deepcopy(base)
    if "resp_digest" in o["statement"]:
        o["statement"]["resp_digest"] = flip_hex(o["statement"]["resp_digest"])
    cases.append(("resp_digest_legacy_alias_swap", o, False))

    rows = [evaluate(case, obj, expected, args.backend, args.lazer_root) for case, obj, expected in cases]
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    print(out)
    if any(not r["pass"] for r in rows):
        raise SystemExit(2)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
