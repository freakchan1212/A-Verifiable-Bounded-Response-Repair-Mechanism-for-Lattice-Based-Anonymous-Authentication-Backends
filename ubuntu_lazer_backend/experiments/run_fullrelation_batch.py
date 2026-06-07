from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

from experiments.run_fullrelation_sha256_smoke import add_v2_digest_fields
from fullzk.generator import InstanceConfig, make_instance
from fullzk.fullrelation import sha256_fullrelation_check
from fullzk.bridge import run_backend

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--backend", choices=["fallback", "lazer"], default="fallback")
    ap.add_argument("--privacy", choices=["committed", "transparent"], default="committed")
    ap.add_argument("--path-mode", choices=["mixed", "repair_only", "no_repair"], default="mixed")
    ap.add_argument("--seeds", type=int, default=1)
    ap.add_argument("--sessions", type=int, default=5)
    ap.add_argument("--out-csv", default="results/v2_fullrelation_batch.csv")
    ap.add_argument("--out-dir", default="build/v2_fullrelation_batch")
    ap.add_argument("--lazer-root", default="third_party/lazer")
    args = ap.parse_args()
    Path(args.out_csv).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out_dir).mkdir(parents=True, exist_ok=True)
    rows = []
    for seed in range(args.seeds):
        for sess in range(args.sessions):
            obj = add_v2_digest_fields(make_instance(InstanceConfig(seed_index=seed, session_index=sess, privacy_mode=args.privacy, path_mode=args.path_mode)))
            sha = sha256_fullrelation_check(obj)
            proof = run_backend(obj, args.backend, args.lazer_root)
            ok = bool(proof.get("verify_ok")) and bool(sha.get("sha256_fullrelation_ok"))
            rows.append({
                "seed": seed,
                "session": sess,
                "backend": args.backend,
                "privacy": args.privacy,
                "path_mode": args.path_mode,
                "verify_ok": ok,
                "proof_ok": proof.get("proof_ok", False),
                "relation_ok": proof.get("relation_ok", False),
                "repair_ok": proof.get("repair_ok", False),
                "binding_ok": proof.get("binding_ok", False),
                "sha256_fullrelation_ok": sha.get("sha256_fullrelation_ok", False),
                "proof_bytes": proof.get("proof_bytes", 0),
                "prove_ms": proof.get("prove_ms", 0.0),
                "verify_ms": proof.get("verify_ms", 0.0),
                "total_ms": proof.get("total_ms", 0.0),
                "sha256_rounds": sum(int(v.get("rounds",0)) for v in sha.get("stats", {}).values()),
                "sha256_est_bool_constraints": sum(int(v.get("estimated_boolean_constraints",0)) for v in sha.get("stats", {}).values()),
                "sha256_est_word_constraints": sum(int(v.get("estimated_word_constraints",0)) for v in sha.get("stats", {}).values()),
            })
    with open(args.out_csv, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader(); writer.writerows(rows)
    failures = sum(1 for r in rows if str(r["verify_ok"]).lower() != "true")
    print(f"wrote {args.out_csv}; failures={failures}/{len(rows)}")
    raise SystemExit(1 if failures else 0)

if __name__ == "__main__":
    main()
