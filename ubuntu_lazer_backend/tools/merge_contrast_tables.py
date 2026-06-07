                      
from __future__ import annotations
import argparse, json
from pathlib import Path

def load(path: Path):
    if path.exists():
        return json.loads(path.read_text(encoding='utf-8'))
    return None

def discover_dirs(root: Path, backend: str):
                                                                      
    for d in sorted(root.glob(f'system_v1_1_{backend}_*_scale')):
        yield d
                                                                     
    for d in sorted(root.glob('system_v1_contrast_*')):
        yield d

def parse_name(name: str, backend: str):
    if name.startswith(f'system_v1_1_{backend}_') and name.endswith('_scale'):
        core = name[len(f'system_v1_1_{backend}_'):-len('_scale')]
    elif name.startswith('system_v1_contrast_'):
        core = name[len('system_v1_contrast_'):]
    else:
        core = name
    for p in ('committed', 'transparent'):
        if core.startswith(p + '_'):
            return p, core[len(p)+1:]
    parts = core.split('_')
    return (parts[0] if parts else ''), ('_'.join(parts[1:]) if len(parts) > 1 else '')

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--results-root', '--root', dest='results_root', default='results')
    ap.add_argument('--backend', default='fallback')
    ap.add_argument('--out-md', required=True)
    ap.add_argument('--out-json', required=True)
    args = ap.parse_args()
    root = Path(args.results_root)
    rows=[]
    seen=set()
    for d in discover_dirs(root, args.backend):
        if d in seen:
            continue
        seen.add(d)
        s=load(d/'paper_results_summary.json')
        if not s:
            continue
        privacy, path_mode = parse_name(d.name, args.backend)
        b=s.get('batch',{})
        leak=s.get('leakage',{})
        rows.append({
            'privacy': privacy,
            'path_mode': path_mode,
            'rows': b.get('rows',0),
            'verify_ok': b.get('verify_ok',''),
            'repair_sessions': b.get('repair_sessions',''),
            'proof_bytes_mean': b.get('proof_bytes',{}).get('mean',0),
            'total_ms_mean': b.get('total_ms',{}).get('mean',0),
            'delta_public_len_mean': leak.get('delta_public_len_mean',0),
            'delta_commitment_len_mean': leak.get('delta_commitment_len_mean',0),
        })
    order={'committed':0,'transparent':1}
    mode_order={'no_repair':0,'repair_only':1,'mixed':2}
    rows.sort(key=lambda r:(order.get(r['privacy'],9), mode_order.get(r['path_mode'],9)))
    Path(args.out_json).parent.mkdir(parents=True,exist_ok=True)
    Path(args.out_json).write_text(json.dumps(rows,indent=2,sort_keys=True)+'\n',encoding='utf-8')
    md=['# System v1 contrast summary\n\n','| Privacy | Path mode | Rows | verify_ok | repair sessions | proof B mean | total ms mean | delta public len mean | delta commitment len mean |\n','|---|---|---:|---:|---:|---:|---:|---:|---:|\n']
    for r in rows:
        md.append(f"| {r['privacy']} | {r['path_mode']} | {r['rows']} | {r['verify_ok']} | {r['repair_sessions']} | {r['proof_bytes_mean']:.3f} | {r['total_ms_mean']:.3f} | {r['delta_public_len_mean']:.3f} | {r['delta_commitment_len_mean']:.3f} |\n")
    Path(args.out_md).write_text(''.join(md),encoding='utf-8')
    print(args.out_md); print(args.out_json)
    return 0
if __name__=='__main__':
    raise SystemExit(main())
