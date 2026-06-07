                      
from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
from typing import Dict, Any

from fullzk.protocol import SystemParams, Issuer, DeviceProver, extract_public_view

def view_without_expected_linkers(view: Dict[str, Any]) -> Dict[str, Any]:
                                                                                
    return {k: v for k, v in view.items() if k not in {"session_id", "commitment", "challenge", "ctx_digest", "repair_digest", "refresh_tag"}}

def predict_same(v1: Dict[str, Any], v2: Dict[str, Any]) -> bool:
                                                                                                                 
    if v1.get("nullifier") == v2.get("nullifier"):
        return True
    if v1.get("credential_tag") == v2.get("credential_tag"):
        return True
    h1 = hashlib.sha256(json.dumps(view_without_expected_linkers(v1), sort_keys=True).encode()).hexdigest()
    h2 = hashlib.sha256(json.dumps(view_without_expected_linkers(v2), sort_keys=True).encode()).hexdigest()
    return h1 == h2

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--trials", type=int, default=100)
    ap.add_argument("--privacy", choices=["committed", "transparent"], default="committed")
    ap.add_argument("--path-mode", choices=["repair_only", "no_repair", "mixed"], default="mixed")
    ap.add_argument("--nullifier-mode", choices=["scope", "unlinkable-demo"], default="unlinkable-demo")
    ap.add_argument("--out-csv", default="results/v03_unlinkability_game.csv")
    args = ap.parse_args()

    Path(args.out_csv).parent.mkdir(parents=True, exist_ok=True)
    params = SystemParams(privacy_mode=args.privacy, nullifier_mode=args.nullifier_mode, path_mode=args.path_mode)
    issuer = Issuer(params)
    cred_a = issuer.issue("device-A")
    cred_b = issuer.issue("device-B")
    prover_a = DeviceProver(params, cred_a)
    prover_b = DeviceProver(params, cred_b)
    correct = 0
    with open(args.out_csv, "w", newline="", encoding="utf-8") as f:
        fields = ["trial", "same_user", "prediction", "correct", "privacy", "path_mode", "nullifier_mode", "note"]
        wr = csv.DictWriter(f, fieldnames=fields); wr.writeheader()
        for i in range(args.trials):
            same = (i % 2 == 0)
            tr1 = prover_a.authenticate(2 * i)
            tr2 = (prover_a if same else prover_b).authenticate(2 * i + 1)
            pred = predict_same(tr1.public_view, tr2.public_view)
            ok = pred == same
            correct += int(ok)
            wr.writerow({
                "trial": i,
                "same_user": same,
                "prediction": pred,
                "correct": ok,
                "privacy": args.privacy,
                "path_mode": args.path_mode,
                "nullifier_mode": args.nullifier_mode,
                "note": "engineering leakage/linkability sanity check; not a formal unlinkability proof",
            })
    print(f"wrote {args.out_csv}; heuristic accuracy={correct / max(args.trials,1):.3f}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
