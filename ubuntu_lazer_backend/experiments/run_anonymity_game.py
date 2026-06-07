                      
from __future__ import annotations

import argparse
import csv
import hashlib
from pathlib import Path

from fullzk.generator import InstanceConfig, make_instance

def public_view(inst):
    st = inst["statement"]
                                                            
    return {
        "credential_commitment": st.get("credential_commitment"),
        "credential_tag": st.get("credential_tag"),
        "nullifier": st.get("nullifier"),
        "delta_commitment": st.get("delta_commitment"),
        "ctx_digest": st.get("ctx_digest"),
        "refresh_tag": st.get("refresh_tag"),
        "repair_digest": st.get("repair_digest"),
        "meta_bytes": inst.get("repair_meta", {}).get("meta_bytes"),
    }

def guess_user(view):
                                                                                                                 
    h = hashlib.sha256(repr(sorted(view.items())).encode()).digest()[0]
    return "A" if h % 2 == 0 else "B"

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--sessions", type=int, default=50)
    ap.add_argument("--privacy", choices=["transparent", "committed"], default="committed")
    ap.add_argument("--path-mode", choices=["repair_only", "no_repair", "mixed"], default="mixed")
    ap.add_argument("--nullifier-mode", choices=["scope", "unlinkable-demo"], default="unlinkable-demo")
    ap.add_argument("--out-csv", default="results/v02_anonymity_game.csv")
    args = ap.parse_args()

    Path(args.out_csv).parent.mkdir(parents=True, exist_ok=True)
    correct = 0
    with open(args.out_csv, "w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=["trial", "chosen_user", "guess", "correct", "privacy", "path_mode", "nullifier_mode", "note"])
        wr.writeheader()
        for i in range(args.sessions):
            chosen = "A" if i % 2 == 0 else "B"
            secret = "user-secret-A" if chosen == "A" else "user-secret-B"
            cfg = InstanceConfig(
                session_index=i,
                privacy_mode=args.privacy,
                path_mode=args.path_mode,
                nullifier_mode=args.nullifier_mode,
                user_secret_tag=secret,
                pseudonym="scope-pseudonym-demo",
            )
            inst = make_instance(cfg)
            view = public_view(inst)
            guess = guess_user(view)
            ok = guess == chosen
            correct += int(ok)
            wr.writerow({
                "trial": i,
                "chosen_user": chosen,
                "guess": guess,
                "correct": ok,
                "privacy": args.privacy,
                "path_mode": args.path_mode,
                "nullifier_mode": args.nullifier_mode,
                "note": "toy implementation sanity check; not a formal anonymity proof",
            })
    acc = correct / max(args.sessions, 1)
    print(f"wrote {args.out_csv}; toy guess accuracy={acc:.3f}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
