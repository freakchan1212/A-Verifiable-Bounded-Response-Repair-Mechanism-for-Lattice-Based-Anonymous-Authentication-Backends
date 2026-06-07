                      
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import sys
import time
from pathlib import Path
from typing import Any, Dict, List

from fullzk.generator import InstanceConfig, make_instance
from fullzk.bridge import get_poly, _poly_const, _poly_from_list

def canonical_json_bytes(obj: Any) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":")).encode("utf-8")

def public_view_minimal(inst: Dict[str, Any]) -> Dict[str, Any]:
    st = inst["statement"]
    return {
        "relation": "minimal_lattice_auth_relation_v1",
        "session_id": st.get("session_id"),
        "commitment": st.get("commitment"),
        "challenge": st.get("challenge"),
        "public_t": st.get("public_t"),
    }

def run_minimal_lazer(inst: Dict[str, Any], lazer_root: str) -> Dict[str, Any]:
    t_all0 = time.perf_counter()
    params = inst["params"]
    witness = inst["witness"]
    statement = inst["statement"]
    deg = int(params["deg"])
    mod = int(params["mod"])
    seed_hex = params.get("seed_hex", "00" * 32)
    seed = bytes.fromhex(seed_hex)
    if len(seed) != 32:
        seed = hashlib.sha256(seed).digest()

    here = Path(__file__).resolve().parents[1] / "lazer_relations_minimal"
    lazer_py = Path(lazer_root).resolve() / "python"
    sys.path.insert(0, str(lazer_py))
    sys.path.insert(0, str(here))
    try:
        import lazer                
        from _auth_minimal_params_cffi import lib                
    except Exception as e:
        raise RuntimeError(
            "Cannot import LaZer or minimal auth params. Build them first with "
            "`(cd lazer_relations_minimal && make LAZER_ROOT=../third_party/lazer)`. "
            f"Original error: {e}"
        )

    prover = lazer.lin_prover_state_t(seed, lib.get_params("auth_minimal_param"))
    verifier = lazer.lin_verifier_state_t(seed, lib.get_params("auth_minimal_param"))
    R = lazer.polyring_t(deg, mod)

    witness_names = ["s0", "s1", "s2", "s3"]
    wvec = lazer.polyvec_t(R, len(witness_names))
    for idx, name in enumerate(witness_names):
        wvec.set_elem(_poly_from_list(lazer, R, get_poly(witness, name, deg)), idx)

    A = lazer.polymat_t(R, 1, 4)
    one = _poly_const(lazer, R, 1)
    two = _poly_const(lazer, R, 2)
    three = _poly_const(lazer, R, 3)
    four = _poly_const(lazer, R, 4)
    A.set_elem(one, 0, 0)
    A.set_elem(two, 0, 1)
    A.set_elem(three, 0, 2)
    A.set_elem(four, 0, 3)

    u = lazer.polyvec_t(R, 1)
    u.set_elem(_poly_from_list(lazer, R, get_poly(statement, "public_t", deg)), 0)

    prover.set_statement(A, u)
    prover.set_witness(wvec)
    t_prove0 = time.perf_counter()
    proof = prover.prove()
    t_prove1 = time.perf_counter()

    verifier.set_statement(A, u)
    t_ver0 = time.perf_counter()
    try:
        verifier.verify(proof)
        proof_ok = True
    except Exception:
        proof_ok = False
    t_ver1 = time.perf_counter()

    pv = public_view_minimal(inst)
    statement_bytes = len(canonical_json_bytes(pv))
    proof_bytes = len(proof)
    return {
        "backend": "lazer_minimal",
        "verify_ok": bool(proof_ok),
        "proof_ok": bool(proof_ok),
        "relation_ok": bool(proof_ok),
        "repair_ok": "not_applicable",
        "binding_ok": "not_applicable",
        "proof_bytes": proof_bytes,
        "statement_bytes": statement_bytes,
        "repair_metadata_bytes": 0,
        "delta_commitment_len": 0,
        "credential_binding_bytes": 0,
        "payload_bytes": proof_bytes + statement_bytes,
        "prove_ms": (t_prove1 - t_prove0) * 1000.0,
        "verify_ms": (t_ver1 - t_ver0) * 1000.0,
        "total_ms": (time.perf_counter() - t_all0) * 1000.0,
        "note": "true minimal LaZer baseline: only the base lattice relation row is proved; no repair relation, no C_delta, no metadata, no digest splice",
    }

def main() -> int:
    ap = argparse.ArgumentParser(description="Reviewer-requested true minimal LaZer baseline")
    ap.add_argument("--seeds", type=int, default=10)
    ap.add_argument("--sessions", type=int, default=200)
    ap.add_argument("--out-csv", default="results/jisa_review_rescue/minimal_lazer_baseline.csv")
    ap.add_argument("--out-dir", default="build/minimal_lazer_baseline")
    ap.add_argument("--lazer-root", default="third_party/lazer")
    args = ap.parse_args()

    Path(args.out_csv).parent.mkdir(parents=True, exist_ok=True)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    fields = [
        "seed", "session", "backend", "verify_ok", "proof_ok", "relation_ok", "repair_ok", "binding_ok",
        "proof_bytes", "statement_bytes", "repair_metadata_bytes", "delta_commitment_len",
        "credential_binding_bytes", "payload_bytes", "prove_ms", "verify_ms", "total_ms", "note",
    ]
    failures = 0
    with open(args.out_csv, "w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=fields)
        wr.writeheader()
        for seed in range(args.seeds):
            for sess in range(args.sessions):
                cfg = InstanceConfig(seed_index=seed, session_index=sess, path_mode="no_repair", privacy_mode="committed")
                inst = make_instance(cfg)
                sd = out_dir / f"seed_{seed:02d}" / f"session_{sess:04d}"
                sd.mkdir(parents=True, exist_ok=True)
                (sd / "minimal_public_statement.json").write_text(json.dumps(public_view_minimal(inst), indent=2, sort_keys=True) + "\n", encoding="utf-8")
                res = run_minimal_lazer(inst, args.lazer_root)
                (sd / "minimal_result.json").write_text(json.dumps(res, indent=2, sort_keys=True) + "\n", encoding="utf-8")
                if not res.get("verify_ok"):
                    failures += 1
                wr.writerow({"seed": seed, "session": sess, **res})
    total = args.seeds * args.sessions
    print(f"wrote {args.out_csv}; failures={failures}/{total}")
    return 0 if failures == 0 else 2

if __name__ == "__main__":
    raise SystemExit(main())
