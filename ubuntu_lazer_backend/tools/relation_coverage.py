                      
from __future__ import annotations
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def render(data: dict) -> str:
    lines = ["# Relation coverage report", "", f"version: {data.get('version','unknown')}", ""]
    if data.get("project_scope"):
        lines += ["## Project scope", data["project_scope"], ""]
    for key, title in [
        ("lazer_proved_current", "LaZer-proved / LaZer-core relations"),
        ("sha256_circuit_checked", "SHA-256 circuit checked relations"),
        ("bridge_verified", "Bridge-verified relations"),
        ("not_yet_machine_checked", "Not yet machine-checked"),
        ("not_yet_claimed", "Not yet claimed"),
    ]:
        vals = data.get(key)
        if vals:
            lines.append(f"## {title}")
            lines.extend(f"- {x}" for x in vals)
            lines.append("")
    if data.get("paper_safe_claim"):
        lines += ["## Paper-safe claim", data["paper_safe_claim"], ""]
    return "\n".join(lines)

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--coverage", default="lazer_relations/relation_coverage_digest_relation.json")
    ap.add_argument("--out", default="")
    args = ap.parse_args()
    p = Path(args.coverage)
    if not p.is_absolute():
        p = ROOT / p
    data = json.loads(p.read_text(encoding="utf-8"))
    text = render(data)
    if args.out:
        Path(args.out).parent.mkdir(parents=True, exist_ok=True)
        Path(args.out).write_text(text + "\n", encoding="utf-8")
    print(text)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
