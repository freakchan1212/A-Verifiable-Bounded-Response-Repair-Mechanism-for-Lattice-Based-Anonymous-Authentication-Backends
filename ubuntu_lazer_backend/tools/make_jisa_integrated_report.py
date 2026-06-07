from __future__ import annotations
import argparse, csv, math
from pathlib import Path
from statistics import mean, median

def rows(p: Path):
    if not p.exists(): return []
    return list(csv.DictReader(p.open(encoding='utf-8')))

def pct(vals, q):
    vals = sorted(vals)
    if not vals: return 0.0
    return vals[max(0, min(len(vals)-1, int(math.ceil(q*len(vals)))-1))]

def batch(name: str, p: Path):
    rs = rows(p); out = [f'## {name}\n']
    if not rs:
        out.append(f'Missing batch file: `{p}`')
        return '\n'.join(out)+'\n'
    n = len(rs); out.append(f'- rows: {n}')
    for f in ['verify_ok','proof_ok','relation_ok','repair_ok','binding_ok','sha256_fullrelation_ok']:
        if f in rs[0]: out.append(f'- {f}: {sum(str(r.get(f,"")).lower()=="true" for r in rs)}/{n}')
    for f in ['proof_bytes','prove_ms','verify_ms','total_ms']:
        if f in rs[0]:
            vals=[]
            for r in rs:
                try:
                    if r.get(f) not in ('',None): vals.append(float(r[f]))
                except Exception: pass
            if vals: out.append(f'- {f}: mean={mean(vals):.3f}, median={median(vals):.3f}, p95={pct(vals,0.95):.3f}, p99={pct(vals,0.99):.3f}')
    return '\n'.join(out)+'\n'

def attack(name: str, p: Path):
    rs = rows(p); out=[f'## {name}\n']
    if not rs:
        out.append(f'Missing attack file: `{p}`')
        return '\n'.join(out)+'\n'
    honest=[r for r in rs if str(r.get('case','')).lower()=='honest']
    attacks=[r for r in rs if str(r.get('case','')).lower()!='honest']
    rejected=sum(str(r.get('verify_ok', r.get('accepted',''))).lower()!='true' for r in attacks)
    out += [f'- honest rows: {len(honest)}', f'- attack rows: {len(attacks)}', f'- rejected attacks: {rejected}/{len(attacks)}']
    return '\n'.join(out)+'\n'

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--artifact-dir', required=True)
    ap.add_argument('--out', default=None)
    args=ap.parse_args()
    art=Path(args.artifact_dir)
    out_path=Path(args.out) if args.out else art/'statistical_report.md'
    md=['# JISA integrated statistical report\n', 'This report integrates the main paper evidence and the S0-S6 robustness/reproducibility layer.\n']
    md.append('## Evidence layers\n')
    md += ['- `main_results/jisa_v1_2_strict_lightweight`: lightweight LaZer-backed committed-offset repair main path.', '- `main_results/jisa_v2_0_sha256_authrelation_strict`: SHA-256 authrelation/fullrelation-checker branch.', '- `main_results/jisa_v2_4_multi_native_extension`: frozen multi-native digest output-limb splice extension.', '- S0-S6 files: workload separation, serialization fuzzing, state rollback/replay, combo tamper, manifest, checksums and environment.\n']
    md.append(batch('jisa_v1_2_strict_lightweight: main result', art/'main_results/jisa_v1_2_strict_lightweight/batch.csv'))
    md.append(attack('jisa_v1_2_strict_lightweight: adversarial result', art/'main_results/jisa_v1_2_strict_lightweight/adversarial_summary.csv'))
    md.append(batch('jisa_v2_0_sha256_authrelation_strict: coverage branch', art/'main_results/jisa_v2_0_sha256_authrelation_strict/batch.csv'))
    md.append(attack('jisa_v2_0_sha256_authrelation_strict: adversarial result', art/'main_results/jisa_v2_0_sha256_authrelation_strict/adversarial_summary.csv'))
    md.append('## jisa_v2_4_multi_native_extension: frozen extension upper bound\n')
    v24=rows(art/'main_results/jisa_v2_4_multi_native_extension/multi_native_splice_summary.csv')
    if v24:
        honest=[r for r in v24 if r.get('case')=='honest']; attacks=[r for r in v24 if r.get('case')!='honest']
        rejected=sum(str(r.get('verify_ok','')).lower()!='true' for r in attacks)
        md += [f'- rows: {len(v24)}', f'- honest rows: {len(honest)}', f'- attack rows: {len(attacks)}', f'- rejected attacks: {rejected}/{len(attacks)}']
        if honest:
            h=honest[0]
            for f in ['proof_bytes','prove_ms','verify_ms','total_ms']:
                if f in h: md.append(f'- honest {f}: {h[f]}')
    else:
        md.append('- Missing v2.4 multi_native_splice_summary.csv')
    md.append('\n## S0-S6 robustness summary\n')
    for label,f in [('workload profile rows','workload_profiles_summary.csv'),('serialization fuzz rows','serialization_fuzz_summary.csv'),('state rollback rows','state_rollback_summary.csv'),('combo tamper rows','combo_tamper_summary.csv'),('multi-native splice rows','multi_native_splice_summary.csv')]:
        md.append(f'- {label}: {len(rows(art/f))}')
    md.append('\n## Interpretation boundary\n')
    md.append('The v1.2 strict result is the main lightweight result. v2.0 and v2.4 are extension branches for relation coverage and coverage-cost trade-off. The v2.4 branch binds multiple SHA-256 digest output limbs into LaZer rows 6..21, while full SHA-256 compression/preimage-to-digest constraints remain in the fullrelation checker/IR path. This report supports a system-level research artifact; it does not claim a production UC-secure anonymous-authentication library.')
    out_path.write_text('\n'.join(md)+'\n', encoding='utf-8')
    print(out_path)

if __name__=='__main__': main()
