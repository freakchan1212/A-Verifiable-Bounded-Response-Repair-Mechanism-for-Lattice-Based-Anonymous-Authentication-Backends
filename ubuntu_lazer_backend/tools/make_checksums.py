from __future__ import annotations
import argparse, hashlib, os
from pathlib import Path

def sha256(p: Path) -> str:
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024), b''):
            h.update(b)
    return h.hexdigest()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--root', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--exclude', action='append', default=[])
    args=ap.parse_args()
    root=Path(args.root)
    out=Path(args.out)
    rows=[]
    for p in sorted(root.rglob('*')):
        if not p.is_file(): continue
        rel=str(p.relative_to(root))
        if rel == out.name or any(x and x in rel for x in args.exclude): continue
        rows.append(f"{sha256(p)}  {rel}")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text('\n'.join(rows)+'\n', encoding='utf-8')
    print(out)
if __name__=='__main__': main()
