from __future__ import annotations

import argparse
import json
from pathlib import Path

from fullzk.generator import InstanceConfig, make_instance
from fullzk.circuits.sha256_bits import sha256_hexdigest
from fullzk.fullrelation import sha256_fullrelation_check, _preimages
from fullzk.bridge import run_backend

def add_v2_digest_fields(obj):
    st = obj["statement"]
                                                                                                
                                                                                        
                                                                                                   
    pre0 = _preimages(obj)
    st["v2_membership_commitment"] = sha256_hexdigest(pre0["v2_membership_commitment"])
    pre1 = _preimages(obj)
    st["v2_membership_tag"] = sha256_hexdigest(pre1["v2_membership_tag"])
    st["v2_nullifier"] = sha256_hexdigest(pre1["v2_nullifier"])
    st["v2_refresh_tag"] = sha256_hexdigest(pre1["v2_refresh_tag"])
    obj["schema"] = "fullzk-auth-lazer-system-v2.0-sha256-authrelation"
    obj["research_boundary"] = {
        "sha256_circuit_layer": "standard SHA-256 bit-level checker is used for commitment, ctx, repair, nullifier, membership-token, and refresh digest relations",
        "lazer_layer": "existing LaZer backend still proves the lightweight algebraic core unless a project-specific SHA-256 LaZer compiler is provided",
        "auth_token_layer": "registration-token style membership witness; not a full general-purpose anonymous credential system",
        "uc_layer": "UC-style ideal functionality and simulator outline are documented; not a machine-checked UC theorem",
    }
    return obj

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--backend", choices=["fallback", "lazer"], default="fallback")
    ap.add_argument("--privacy", choices=["committed", "transparent"], default="committed")
    ap.add_argument("--path-mode", choices=["mixed", "repair_only", "no_repair"], default="mixed")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--session", type=int, default=0)
    ap.add_argument("--out-dir", default="build/fullrelation_sha256_smoke")
    ap.add_argument("--lazer-root", default="third_party/lazer")
    args = ap.parse_args()

    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    cfg = InstanceConfig(seed_index=args.seed, session_index=args.session, privacy_mode=args.privacy, path_mode=args.path_mode)
    obj = add_v2_digest_fields(make_instance(cfg))
    (out / "statement_witness.json").write_text(json.dumps(obj, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    sha = sha256_fullrelation_check(obj)
    proof = run_backend(obj, args.backend, args.lazer_root)
    result = {
        "backend": args.backend,
        "verify_ok": bool(proof.get("verify_ok")) and bool(sha.get("sha256_fullrelation_ok")),
        "proof_result": proof,
        "sha256_fullrelation": sha,
        "note": "v2.0 SHA-256 fullrelation checker exercises full digest/nullifier/membership/refresh preimages; existing LaZer backend is reused for algebraic core unless extended with a SHA-256 compiler.",
    }
    (out / "fullrelation_result.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(out / "fullrelation_result.json")
    if not result["verify_ok"]:
        raise SystemExit(2)

if __name__ == "__main__":
    main()
