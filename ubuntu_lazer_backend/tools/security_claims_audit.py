                      
from __future__ import annotations
import argparse
from pathlib import Path

BANNED = [
    "production-ready",
    "production deployment",
    "full anonymous credential system is proved",
    "all relations are proved in LaZer",
    "formal anonymity proof completed",
    "formal zero-knowledge proof completed",
]
REQUIRED = [
    "bridge-level verifier",
    "committed-offset repair relation",
    "research implementation",
]

def main() -> int:
    ap = argparse.ArgumentParser(description="Audit paper/docs for over-claiming phrases.")
    ap.add_argument("paths", nargs="+", help="Markdown/text files to audit")
    args = ap.parse_args()
    bad = 0
    for p0 in args.paths:
        p = Path(p0)
        if not p.exists():
            print(f"missing: {p}")
            bad += 1
            continue
        text = p.read_text(encoding="utf-8", errors="ignore")
        lower = text.lower()
        for b in BANNED:
            if b.lower() in lower:
                print(f"OVERCLAIM? {p}: contains '{b}'")
                bad += 1
        missing = [r for r in REQUIRED if r.lower() not in lower]
        if missing:
            print(f"NOTE {p}: missing recommended boundary phrase(s): {missing}")
    return 1 if bad else 0

if __name__ == "__main__":
    raise SystemExit(main())
