                      
from __future__ import annotations

import argparse
from pathlib import Path

from fullzk.generator import InstanceConfig, dump_instance, make_instance

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--session-index", type=int, default=0)
    ap.add_argument("--epoch", type=int, default=0)
    ap.add_argument("--privacy", choices=["transparent", "committed"], default="committed")
    ap.add_argument("--path-mode", choices=["repair_only", "no_repair", "mixed"], default="mixed")
    ap.add_argument("--seed-index", type=int, default=0)
    ap.add_argument("--deg", type=int, default=256)
    ap.add_argument("--B", type=int, default=0)
    ap.add_argument("--user-secret-tag", default="user-secret-demo")
    ap.add_argument("--scope", default="edge-gateway-auth")
    ap.add_argument("--pseudonym", default="scope-pseudonym-demo")
    ap.add_argument("--nullifier-mode", choices=["scope", "unlinkable-demo"], default="scope")
    args = ap.parse_args()
    cfg = InstanceConfig(
        deg=args.deg,
        B=args.B,
        epoch=args.epoch,
        session_index=args.session_index,
        seed_index=args.seed_index,
        privacy_mode=args.privacy,
        path_mode=args.path_mode,
        user_secret_tag=args.user_secret_tag,
        scope=args.scope,
        pseudonym=args.pseudonym,
        nullifier_mode=args.nullifier_mode,
    )
    inst = make_instance(cfg)
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    dump_instance(args.out, inst)
    print(args.out)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
