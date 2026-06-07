                      
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

from fullzk.protocol import SystemParams, Issuer, DeviceProver, EdgeVerifier

def main() -> int:
    ap = argparse.ArgumentParser(description="Honest-path full-system runner")
    ap.add_argument("--backend", choices=["fallback", "lazer"], default="fallback")
    ap.add_argument("--sessions", type=int, default=20)
    ap.add_argument("--seeds", type=int, default=1)
    ap.add_argument("--privacy", choices=["committed", "transparent"], default="committed")
    ap.add_argument("--path-mode", choices=["repair_only", "no_repair", "mixed"], default="mixed")
    ap.add_argument("--nullifier-mode", choices=["scope", "unlinkable-demo"], default="unlinkable-demo")
    ap.add_argument("--out-dir", default="build/system_v1_honest")
    ap.add_argument("--out-csv", default="results/system_v1_honest_path.csv")
    ap.add_argument("--lazer-root", default="third_party/lazer")
    args = ap.parse_args()

    out_dir = Path(args.out_dir); out_dir.mkdir(parents=True, exist_ok=True)
    Path(args.out_csv).parent.mkdir(parents=True, exist_ok=True)
    params = SystemParams(privacy_mode=args.privacy, nullifier_mode=args.nullifier_mode, path_mode=args.path_mode)
    verifier = EdgeVerifier(params, backend=args.backend, lazer_root=args.lazer_root)

    fields = ["seed", "session", "backend", "privacy", "path_mode", "B", "nullifier_mode", "verify_ok", "proof_ok", "relation_ok", "repair_ok", "binding_ok", "proof_bytes", "prove_ms", "verify_ms", "total_ms", "repair_flag", "delta_l0", "public_view_hash", "failed_checks"]
    failures = 0
    total = args.seeds * args.sessions
    with open(args.out_csv, "w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=fields); wr.writeheader()
        for seed in range(args.seeds):
            cred = Issuer(params).issue(f"device-A-seed-{seed}")
            prover = DeviceProver(params, cred)
            for i in range(args.sessions):
                tr = prover.authenticate(i)
                res = verifier.verify(tr)
                sd = out_dir / f"seed_{seed:02d}" / f"session_{i:04d}"; sd.mkdir(parents=True, exist_ok=True)
                (sd / "statement_witness.json").write_text(json.dumps(tr.statement_witness, indent=2, sort_keys=True) + "\n", encoding="utf-8")
                (sd / "public_view.json").write_text(json.dumps(tr.public_view, indent=2, sort_keys=True) + "\n", encoding="utf-8")
                (sd / "verify_result.json").write_text(json.dumps(res, indent=2, sort_keys=True) + "\n", encoding="utf-8")
                failures += 0 if res.get("verify_ok") else 1
                meta = tr.statement_witness.get("repair_meta", {})
                wr.writerow({
                    "seed": seed,
                    "session": i,
                    "backend": res.get("backend"),
                    "privacy": args.privacy,
                    "path_mode": args.path_mode,
                    "B": tr.statement_witness.get("params", {}).get("B"),
                    "nullifier_mode": args.nullifier_mode,
                    "verify_ok": res.get("verify_ok"),
                    "proof_ok": res.get("proof_ok"),
                    "relation_ok": res.get("relation_ok"),
                    "repair_ok": res.get("repair_ok"),
                    "binding_ok": res.get("binding_ok"),
                    "proof_bytes": res.get("proof_bytes", 0),
                    "prove_ms": res.get("prove_ms", 0.0),
                    "verify_ms": res.get("verify_ms", 0.0),
                    "total_ms": res.get("total_ms", 0.0),
                    "repair_flag": meta.get("repair_flag", 0),
                    "delta_l0": meta.get("delta_l0", 0),
                    "public_view_hash": tr.public_view.get("repair_digest"),
                    "failed_checks": ";".join(res.get("failed_checks", [])),
                })
    print(f"wrote {args.out_csv}; failures={failures}/{total}")
    return 0 if failures == 0 else 2

if __name__ == "__main__":
    raise SystemExit(main())
