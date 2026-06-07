from __future__ import annotations
import argparse, json
from pathlib import Path

PARAMS=[
    {'group':'main','name_cn':'主参数组','N':256,'q':12289,'k':3,'B_loose':8,'B_medium':4,'B_tight':3,'role':'repair-layer primary group'},
    {'group':'aligned','name_cn':'对齐参数组','N':256,'q':3329,'k':3,'B_loose':8,'B_medium':4,'B_tight':3,'role':'ML-KEM-like alignment group'},
    {'group':'v2.4-native-splice','name_cn':'v2.4扩展关系组','N':256,'q':'LaZer generated parameter','k':'relation dependent','B':'mixed workload effective bound','role':'extension experiment upper bound'},
]

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--out-dir', required=True); args=ap.parse_args()
    out=Path(args.out_dir); out.mkdir(parents=True, exist_ok=True)
    md=['# Concrete parameter and encoding table','', '| Group | 中文名 | N | q | k | Bounds | Role |','|---|---|---:|---:|---:|---|---|']
    for r in PARAMS:
        bounds=f"loose={r.get('B_loose','')}, medium={r.get('B_medium','')}, tight={r.get('B_tight','')}" if 'B_loose' in r else str(r.get('B',''))
        md.append(f"| {r['group']} | {r['name_cn']} | {r['N']} | {r['q']} | {r['k']} | {bounds} | {r['role']} |")
    md += ['', '## Encoding and domain separation', '', '- Coefficients are interpreted in an integer-lifted domain before clipping.', '- `clip_B(x)=min(B,max(-B,x))` is applied coefficient-wise.', '- SHA-256 domain tags are fixed in `configs/encoding_and_domain_separation.json`.', '- v2.4 native-splice binds digest output limbs, not SHA-256 compression rounds.']
    (out/'concrete_parameter_table.md').write_text('\n'.join(md)+'\n', encoding='utf-8')
    (out/'concrete_parameter_table.json').write_text(json.dumps(PARAMS, indent=2, ensure_ascii=False), encoding='utf-8')
    print(out/'concrete_parameter_table.md')
if __name__=='__main__': main()
