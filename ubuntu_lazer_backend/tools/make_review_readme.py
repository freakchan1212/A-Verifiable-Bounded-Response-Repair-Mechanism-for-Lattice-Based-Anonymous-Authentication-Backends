from __future__ import annotations
import argparse
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--out', required=True); args=ap.parse_args()
    text='''# Review artifact README

This artifact supports the paper revision for a LaZer-backed lattice-based anonymous authentication backend with verifier-checkable bounded-response repair.

## Recommended reproduction commands

```bash
bash scripts/run_s0_s6_system_artifact.sh
```

For real LaZer experiments, copy or clone LaZer into `third_party/lazer`, rebuild relation parameters in `lazer_relations/`, and run:

```bash
BACKEND=lazer OUT=results/s0_s6_lazer bash scripts/run_s0_s6_system_artifact.sh
```

## Main interpretation

- v1.2 strict remains the lightweight main path.
- v2.4 is frozen as the fullrelation/native-splice extension upper bound.
- v2.4 binds multiple digest output limbs into the LaZer proof path, while SHA-256 compression/preimage correctness remains in the fullrelation checker/IR path.

## Files

- `artifact_manifest.json`: artifact version and scope.
- `environment.json`: OS/compiler/Python/LaZer metadata.
- `checksums.sha256`: file checksums.
- `concrete_parameter_table.md`: parameter and encoding summary.
- `statistical_report.md`: batch and tamper summary statistics.
- `declarations_template.md`: manuscript declaration text.
'''
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out).write_text(text, encoding='utf-8')
    print(args.out)
if __name__=='__main__': main()
