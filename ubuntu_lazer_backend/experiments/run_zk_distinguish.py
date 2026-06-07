                      
from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path

from fullzk.protocol import SystemParams, Issuer, DeviceProver
from fullzk.crypto_utils import fixed_commit

def simulated_public_view(real_view, trial: int):
                                                                                                                                        
    sim = dict(real_view)
    for k, v in list(sim.items()):
        if isinstance(v, str) and len(v) >= 32:
            sim[k] = hashlib.sha256(f"sim|{trial}|{k}|{len(v)}".encode()).hexdigest()[:len(v)]
    return sim

def simple_label_guess(view):
    h = hashlib.sha256(json.dumps(view, sort_keys=True).encode()).digest()[0]
    return "real" if h % 2 == 0 else "sim"

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--trials", type=int, default=100)
    ap.add_argument("--privacy", choices=["committed", "transparent"], default="committed")
    ap.add_argument("--path-mode", choices=["repair_only", "no_repair", "mixed"], default="mixed")
    ap.add_argument("--out-csv", default="results/v03_zk_distinguish_sanity.csv")
    args = ap.parse_args()

    Path(args.out_csv).parent.mkdir(parents=True, exist_ok=True)
    params = SystemParams(privacy_mode=args.privacy, nullifier_mode="unlinkable-demo", path_mode=args.path_mode)
    cred = Issuer(params).issue("device-A")
    prover = DeviceProver(params, cred)
    correct = 0
    total = 0
    with open(args.out_csv, "w", newline="", encoding="utf-8") as f:
        fields = ["trial", "label", "guess", "correct", "privacy", "path_mode", "note"]
        wr = csv.DictWriter(f, fieldnames=fields); wr.writeheader()
        for i in range(args.trials):
            real = prover.authenticate(i).public_view
            views = [("real", real), ("sim", simulated_public_view(real, i))]
            for label, view in views:
                guess = simple_label_guess(view)
                ok = guess == label
                correct += int(ok); total += 1
                wr.writerow({"trial": i, "label": label, "guess": guess, "correct": ok, "privacy": args.privacy, "path_mode": args.path_mode, "note": "simulator-format sanity check only; not a cryptographic ZK proof"})
    print(f"wrote {args.out_csv}; toy classifier accuracy={correct / max(total,1):.3f}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
