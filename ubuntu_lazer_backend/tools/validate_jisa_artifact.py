                      
from __future__ import annotations
import argparse, sys
from pathlib import Path
REQUIRED=[
 'raw/boundary_hit_probability.csv','raw/abort_retry_session.csv','derived/abort_retry_summary.csv',
 'derived/local_clipping_baseline_summary.csv','derived/binding_ablation_summary.csv','derived/repair_metadata_leakage_summary.csv',
 'system_lazer_committed_mixed/batch.csv','system_lazer_committed_mixed/adversarial_summary.csv',
 'system_lazer_committed_mixed/leakage_metrics.csv','paper_tables.md','paper_results_summary.json','figures/figure_manifest.md'
]
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--root',default='results/jisa_paper_v1_2'); args=ap.parse_args(); root=Path(args.root)
    missing=[x for x in REQUIRED if not (root/x).exists()]
    if missing:
        print('Missing required outputs:'); [print(' -',m) for m in missing]; return 2
    figs=list((root/'figures').glob('*.png'))
    print(f'Artifact OK: {root}; figures={len(figs)}')
    return 0
if __name__=='__main__': raise SystemExit(main())
