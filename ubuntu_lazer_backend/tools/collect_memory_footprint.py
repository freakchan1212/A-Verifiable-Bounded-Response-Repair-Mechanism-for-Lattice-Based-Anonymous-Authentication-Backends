                      
from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path
from typing import Any, Dict, List

def parse_timev(path: Path) -> Dict[str, Any]:
    text = path.read_text(encoding="utf-8", errors="ignore") if path.exists() else ""
    def find(pattern: str, default: str = "") -> str:
        m = re.search(pattern, text)
        return m.group(1).strip() if m else default
    return {
        "timev_file": str(path),
        "command": find(r"Command being timed:\s*\"(.*?)\""),
        "elapsed_s_raw": find(r"Elapsed \(wall clock\) time.*?:\s*(.*)"),
        "user_s": find(r"User time \(seconds\):\s*([0-9.]+)"),
        "system_s": find(r"System time \(seconds\):\s*([0-9.]+)"),
        "peak_rss_kb": find(r"Maximum resident set size \(kbytes\):\s*(\d+)", "0"),
    }

def label_from_name(name: str) -> str:
    return {
        "minimal_lazer": "true minimal LaZer baseline",
        "committed_no_repair": "zero-offset committed control",
        "committed_repair_only": "repair-enabled committed path",
        "committed_mixed": "controlled mixed committed path",
        "fullrelation": "SHA-256 fullrelation extension",
        "multi_native_splice": "output-limb splice upper-bound",
    }.get(name, name)

def main() -> int:
    ap = argparse.ArgumentParser(description="Collect /usr/bin/time -v peak RSS logs for JISA final commands")
    ap.add_argument("--logs", default="results/final_experiments/logs")
    ap.add_argument("--out", default="results/final_experiments/derived/memory_footprint.csv")
    args = ap.parse_args()
    logs = Path(args.logs)
    rows: List[Dict[str, Any]] = []
    for p in sorted(logs.glob("*.timev")):
        stem = p.stem
        data = parse_timev(p)
        peak = int(data.get("peak_rss_kb") or 0)
        data.update({
            "path_label": label_from_name(stem),
            "sample_kind": stem,
            "max_rss_mb": f"{peak/1024.0:.3f}",
        })
        rows.append(data)
    out = Path(args.out); out.parent.mkdir(parents=True, exist_ok=True)
    fields = ["path_label", "sample_kind", "peak_rss_kb", "max_rss_mb", "elapsed_s_raw", "user_s", "system_s", "command", "timev_file"]
    with out.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader(); w.writerows(rows)
    print(out)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
