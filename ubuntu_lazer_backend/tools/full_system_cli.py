                      
from __future__ import annotations

import argparse
import json
from dataclasses import asdict
from pathlib import Path

from fullzk.protocol import SystemParams, Issuer, DeviceProver, EdgeVerifier, extract_public_view, setup_demo

def write_json(path: str, obj) -> None:
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f, indent=2, sort_keys=True, ensure_ascii=False)
        f.write("\n")

def load_json(path: str):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

def build_params(args) -> SystemParams:
    return SystemParams(
        epoch=args.epoch,
        privacy_mode=args.privacy,
        nullifier_mode=args.nullifier_mode,
        scope=args.scope,
        path_mode=args.path_mode,
    )

def cmd_describe(args) -> int:
    print(json.dumps({
        "project": "fullzk-auth-lazer-system-ubuntu-v1.0",
        "pipeline": ["Setup", "Issue", "Auth/Show", "Repair", "Prove", "Verify", "Refresh"],
        "lazer_layer": ["As=t", "z_raw = z_repaired + Delta"],
        "bridge_layer": ["ctx_digest", "refresh_tag", "repair_digest", "credential_commitment", "credential_tag", "nullifier", "delta_commitment", "metadata"],
        "system_status": "Executable complete-system research implementation: Setup/Issue/Auth/Repair/Prove/Verify/Refresh plus mature artifact matrix.",
        "boundary": "Credential/hash/nullifier bindings are enforced by the bridge-level verifier in v1.0; LaZer proves the core lattice and repair relations.",
    }, indent=2, ensure_ascii=False))
    return 0

def cmd_demo(args) -> int:
    params = build_params(args)
    obj = setup_demo(params)
    write_json(args.out, obj)
    print(args.out)
    return 0

def cmd_issue(args) -> int:
    params = build_params(args)
    cred = Issuer(params).issue(args.user_id, {"role": args.role, "valid": True})
    write_json(args.out, {"params": asdict(params), "credential": asdict(cred)})
    print(args.out)
    return 0

def cmd_auth(args) -> int:
    bundle = load_json(args.credential)
    params = SystemParams(**{k: v for k, v in bundle.get("params", {}).items() if k in SystemParams.__dataclass_fields__})
                                                       
    params.privacy_mode = args.privacy
    params.nullifier_mode = args.nullifier_mode
    params.epoch = args.epoch
    cred_data = bundle["credential"]
    from fullzk.protocol import Credential
    cred = Credential(**cred_data)
    tr = DeviceProver(params, cred).authenticate(args.session)
    write_json(args.out, tr.statement_witness)
    write_json(args.public_out, tr.public_view)
    print(args.out)
    return 0

def cmd_verify(args) -> int:
    inst = load_json(args.input)
    params = build_params(args)
    res = EdgeVerifier(params, backend=args.backend, lazer_root=args.lazer_root).verify(type("T", (), {"statement_witness": inst, "verifier_result": None})())
    write_json(args.out, res)
    print(args.out)
    return 0 if res.get("verify_ok") else 2

def main() -> int:
    ap = argparse.ArgumentParser(description="Full-system protocol CLI for the LaZer-backed auth-repair prototype")
    sub = ap.add_subparsers(dest="cmd", required=True)

    def common(p):
        p.add_argument("--privacy", choices=["committed", "transparent"], default="committed")
        p.add_argument("--path-mode", choices=["repair_only", "no_repair", "mixed"], default="mixed")
        p.add_argument("--nullifier-mode", choices=["scope", "unlinkable-demo"], default="scope")
        p.add_argument("--epoch", type=int, default=0)
        p.add_argument("--scope", default="edge-gateway-auth")

    p = sub.add_parser("describe")
    p.set_defaults(func=cmd_describe)

    p = sub.add_parser("demo")
    common(p)
    p.add_argument("--out", default="build/full_system_demo.json")
    p.set_defaults(func=cmd_demo)

    p = sub.add_parser("issue")
    common(p)
    p.add_argument("--user-id", default="device-A")
    p.add_argument("--role", default="device")
    p.add_argument("--out", default="build/credential_A.json")
    p.set_defaults(func=cmd_issue)

    p = sub.add_parser("auth")
    common(p)
    p.add_argument("--credential", default="build/credential_A.json")
    p.add_argument("--session", type=int, default=0)
    p.add_argument("--out", default="build/statement_witness.json")
    p.add_argument("--public-out", default="build/public_view.json")
    p.set_defaults(func=cmd_auth)

    p = sub.add_parser("verify")
    common(p)
    p.add_argument("--backend", choices=["fallback", "lazer"], default="fallback")
    p.add_argument("--lazer-root", default="third_party/lazer")
    p.add_argument("--input", default="build/statement_witness.json")
    p.add_argument("--out", default="build/verify_result.json")
    p.set_defaults(func=cmd_verify)

    args = ap.parse_args()
    return args.func(args)

if __name__ == "__main__":
    raise SystemExit(main())
