                      
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any

from fullzk.bridge import run_backend
from fullzk.generator import InstanceConfig, make_instance

def canonical_json_bytes(obj: Any) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":")).encode("utf-8")

def public_z_saturation(z: list[Any], B: int) -> dict:
    """Count public-z boundary saturation with a well-defined B=0 convention.

    For B>0, +B and -B are distinct faces.  For B=0, the two faces coincide,
    so every zero coefficient is counted once as `saturated_pos` and never again
    as `saturated_neg`.  This keeps saturated_ratio in [0,1].
    """
    coeffs = len(z)
    if not z:
        return {"saturated_pos": 0, "saturated_neg": 0, "saturated_total": 0, "saturated_ratio": 0.0}
    if B == 0:
        saturated_pos = sum(1 for x in z if int(x) == 0)
        saturated_neg = 0
    else:
        saturated_pos = sum(1 for x in z if int(x) == B)
        saturated_neg = sum(1 for x in z if int(x) == -B)
    saturated_total = saturated_pos + saturated_neg
    return {
        "saturated_pos": saturated_pos,
        "saturated_neg": saturated_neg,
        "saturated_total": saturated_total,
        "saturated_ratio": (saturated_total / coeffs) if coeffs else 0.0,
    }

def payload_stats(inst: dict, proof_bytes: int) -> dict:
    st = inst.get("statement", {})
    public_inputs = inst.get("public_inputs", {})
    meta = inst.get("repair_meta", {})
    params = inst.get("params", {})
    z = st.get("z_public", []) or []
    B = int(params.get("B", 0) or 0)
    sat = public_z_saturation(z, B)
    statement_bytes = len(canonical_json_bytes(st))
    public_inputs_bytes = len(canonical_json_bytes(public_inputs))
    repair_metadata_bytes = len(canonical_json_bytes(meta))
    delta_commitment_len = len(st.get("delta_commitment", "") or "")
    credential_binding_bytes = len((st.get("credential_commitment", "") or "")) + len((st.get("credential_tag", "") or "")) + len((st.get("nullifier", "") or ""))
    return {
        "saturated_pos": sat["saturated_pos"],
        "saturated_neg": sat["saturated_neg"],
        "saturated_total": sat["saturated_total"],
        "saturated_ratio": sat["saturated_ratio"],
        "statement_bytes": statement_bytes,
        "public_inputs_bytes": public_inputs_bytes,
        "repair_metadata_bytes": repair_metadata_bytes,
        "delta_commitment_len": delta_commitment_len,
        "credential_binding_bytes": credential_binding_bytes,
        "payload_bytes": proof_bytes + statement_bytes + public_inputs_bytes + repair_metadata_bytes,
    }

def main() -> int:
    ap = argparse.ArgumentParser(description="Mature full-system batch runner with seed/session/path-mode support")
    ap.add_argument("--backend", choices=["fallback", "lazer"], default="fallback")
    ap.add_argument("--sessions", type=int, default=20)
    ap.add_argument("--seeds", type=int, default=1)
    ap.add_argument("--epoch", type=int, default=0)
    ap.add_argument("--privacy", choices=["transparent", "committed"], default="committed")
    ap.add_argument("--path-mode", choices=["repair_only", "no_repair", "mixed"], default="mixed")
    ap.add_argument("--out-csv", default="results/system_v1_batch.csv")
    ap.add_argument("--out-dir", default="build/system_v1_batch")
    ap.add_argument("--lazer-root", default="third_party/lazer")
    ap.add_argument("--nullifier-mode", choices=["scope", "unlinkable-demo"], default="unlinkable-demo")
    args = ap.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    Path(args.out_csv).parent.mkdir(parents=True, exist_ok=True)

    fields = [
        "seed", "session", "global_index", "backend", "privacy", "path_mode", "B", "nullifier_mode",
        "verify_ok", "proof_ok", "relation_ok", "repair_ok", "binding_ok",
        "repair_flag", "repair_count", "delta_l0", "delta_linf", "z_linf", "meta_bytes",
        "saturated_pos", "saturated_neg", "saturated_total", "saturated_ratio",
        "delta_commitment_len", "statement_bytes", "public_inputs_bytes",
        "repair_metadata_bytes", "credential_binding_bytes", "payload_bytes",
        "proof_bytes", "prove_ms", "verify_ms", "total_ms", "failed_checks", "note"
    ]
    failures = 0
    total = args.seeds * args.sessions
    with open(args.out_csv, "w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=fields)
        wr.writeheader()
        g = 0
        for seed in range(args.seeds):
            for i in range(args.sessions):
                cfg = InstanceConfig(
                    session_index=i,
                    seed_index=seed,
                    epoch=args.epoch,
                    privacy_mode=args.privacy,
                    path_mode=args.path_mode,
                    nullifier_mode=args.nullifier_mode,
                )
                inst = make_instance(cfg)
                session_dir = out_dir / f"seed_{seed:02d}" / f"session_{i:04d}"
                session_dir.mkdir(parents=True, exist_ok=True)
                in_path = session_dir / "statement_witness.json"
                out_path = session_dir / "bridge_result.json"
                in_path.write_text(json.dumps(inst, indent=2, sort_keys=True) + "\n", encoding="utf-8")
                res = run_backend(inst, args.backend, args.lazer_root)
                out_path.write_text(json.dumps(res, indent=2, sort_keys=True) + "\n", encoding="utf-8")
                if not res.get("verify_ok"):
                    failures += 1
                meta = inst.get("repair_meta", {})
                ps = payload_stats(inst, int(res.get("proof_bytes", 0) or 0))
                wr.writerow({
                    "seed": seed,
                    "session": i,
                    "global_index": g,
                    "backend": res.get("backend"),
                    "privacy": args.privacy,
                    "path_mode": args.path_mode,
                    "B": inst.get("params", {}).get("B"),
                    "nullifier_mode": args.nullifier_mode,
                    "verify_ok": res.get("verify_ok"),
                    "proof_ok": res.get("proof_ok"),
                    "relation_ok": res.get("relation_ok"),
                    "repair_ok": res.get("repair_ok"),
                    "binding_ok": res.get("binding_ok"),
                    "repair_flag": meta.get("repair_flag", 0),
                    "repair_count": meta.get("repair_count", 0),
                    "delta_l0": meta.get("delta_l0", 0),
                    "delta_linf": meta.get("delta_linf", 0),
                    "z_linf": meta.get("z_linf", 0),
                    "meta_bytes": meta.get("meta_bytes", 0),
                    "saturated_pos": ps["saturated_pos"],
                    "saturated_neg": ps["saturated_neg"],
                    "saturated_total": ps["saturated_total"],
                    "saturated_ratio": f"{ps['saturated_ratio']:.8f}",
                    "delta_commitment_len": ps["delta_commitment_len"],
                    "statement_bytes": ps["statement_bytes"],
                    "public_inputs_bytes": ps["public_inputs_bytes"],
                    "repair_metadata_bytes": ps["repair_metadata_bytes"],
                    "credential_binding_bytes": ps["credential_binding_bytes"],
                    "payload_bytes": ps["payload_bytes"],
                    "proof_bytes": res.get("proof_bytes", 0),
                    "prove_ms": res.get("prove_ms", 0.0),
                    "verify_ms": res.get("verify_ms", 0.0),
                    "total_ms": res.get("total_ms", 0.0),
                    "failed_checks": ";".join(res.get("failed_checks", [])),
                    "note": res.get("note", ""),
                })
                g += 1
    print(f"wrote {args.out_csv}; failures={failures}/{total}")
    return 0 if failures == 0 else 2

if __name__ == "__main__":
    raise SystemExit(main())
