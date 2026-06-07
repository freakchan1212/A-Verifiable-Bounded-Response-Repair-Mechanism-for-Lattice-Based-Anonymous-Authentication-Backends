                      
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

def run(cmd):
    print("[run]", " ".join(cmd), flush=True)
    env = os.environ.copy()
    here = str(Path.cwd())
    env["PYTHONPATH"] = here + (":" + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
    return subprocess.call(cmd, env=env)

def main() -> int:
    ap = argparse.ArgumentParser(description="One-command mature full-system artifact runner")
    ap.add_argument("--backend", choices=["fallback", "lazer"], default="fallback")
    ap.add_argument("--sessions", type=int, default=20)
    ap.add_argument("--seeds", type=int, default=3)
    ap.add_argument("--trials", type=int, default=100)
    ap.add_argument("--privacy", choices=["committed", "transparent"], default="committed")
    ap.add_argument("--path-mode", choices=["repair_only", "no_repair", "mixed"], default="mixed")
    ap.add_argument("--lazer-root", default="third_party/lazer")
    ap.add_argument("--out-dir", default="results/system_v1_mature_artifact")
    args = ap.parse_args()
    out = Path(args.out_dir); out.mkdir(parents=True, exist_ok=True)
    py = sys.executable
    build_prefix = "build/system_v1_" + args.backend + "_" + args.privacy + "_" + args.path_mode
    cmds = [
        [py, "experiments/run_honest_path.py", "--backend", args.backend, "--privacy", args.privacy, "--path-mode", args.path_mode, "--seeds", str(args.seeds), "--sessions", str(args.sessions), "--out-csv", str(out / "honest_path.csv"), "--out-dir", build_prefix + "_honest", "--lazer-root", args.lazer_root],
        [py, "experiments/run_batch.py", "--backend", args.backend, "--privacy", args.privacy, "--path-mode", args.path_mode, "--seeds", str(args.seeds), "--sessions", str(args.sessions), "--out-csv", str(out / "batch.csv"), "--out-dir", build_prefix + "_batch", "--lazer-root", args.lazer_root],
        [py, "experiments/run_adversarial.py", "--backend", args.backend, "--privacy", args.privacy, "--path-mode", "repair_only", "--out-dir", build_prefix + "_adversarial", "--lazer-root", args.lazer_root],
        [py, "experiments/run_anonymity_game.py", "--sessions", str(args.trials), "--privacy", args.privacy, "--path-mode", args.path_mode, "--out-csv", str(out / "anonymity_game.csv")],
        [py, "experiments/run_unlinkability_game.py", "--trials", str(args.trials), "--privacy", args.privacy, "--path-mode", args.path_mode, "--out-csv", str(out / "unlinkability_game.csv")],
        [py, "experiments/run_zk_distinguish.py", "--trials", str(args.trials), "--privacy", args.privacy, "--path-mode", args.path_mode, "--out-csv", str(out / "zk_distinguish_sanity.csv")],
        [py, "experiments/run_leakage_metrics.py", "--seeds", str(args.seeds), "--sessions", str(args.trials), "--privacy", args.privacy, "--path-mode", args.path_mode, "--out-csv", str(out / "leakage_metrics.csv"), "--summary", str(out / "leakage_summary.txt")],
    ]
    rc = 0
    for cmd in cmds:
        r = run(cmd)
        if r != 0:
            rc = r
            if args.backend == "lazer":
                break
    src = Path(build_prefix + "_adversarial") / "adversarial_summary.csv"
    if src.exists():
        (out / "adversarial_summary.csv").write_text(src.read_text(encoding="utf-8"), encoding="utf-8")
    if rc == 0:
        run([py, "tools/make_paper_tables.py", "--artifact-dir", str(out), "--out-md", str(out / "paper_tables.md"), "--out-json", str(out / "paper_results_summary.json")])
    print(f"artifact dir: {out}")
    return rc

if __name__ == "__main__":
    raise SystemExit(main())
