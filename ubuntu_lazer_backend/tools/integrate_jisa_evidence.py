from __future__ import annotations

import argparse
import csv
import json
import math
import shutil
from pathlib import Path
from statistics import mean, median
from typing import Iterable

def read_csv(path: Path):
    if not path.exists():
        return []
    with path.open(encoding='utf-8') as f:
        return list(csv.DictReader(f))

def pctile(vals, q):
    vals = sorted(vals)
    if not vals:
        return None
    k = int(math.ceil(q * len(vals))) - 1
    return vals[max(0, min(k, len(vals)-1))]

def copy_first(label: str, candidates: Iterable[Path], dst_dir: Path, dst_name: str | None = None, log: list[str] | None = None):
    dst_dir.mkdir(parents=True, exist_ok=True)
    for c in candidates:
        if c and c.exists() and c.is_file():
            dst = dst_dir / (dst_name or c.name)
            shutil.copy2(c, dst)
            msg = f'[OK] {label}: {c} -> {dst}'
            print(msg)
            if log is not None:
                log.append(msg)
            return dst
    msg = f'[MISS] {label}: no candidate found'
    print(msg)
    if log is not None:
        log.append(msg)
    return None

def find_candidates(base: Path, patterns: list[str]) -> list[Path]:
    """Return explicit candidate paths only.

    This JISA runner intentionally avoids broad recursive ** searches so the
    artifact script stays predictable in large project folders. Patterns are
    interpreted relative to the current project root and its parent.
    """
    out: list[Path] = []
    for pat in patterns:
        out.append(base / pat)
        out.append(base.parent / pat)
    seen = set(); uniq=[]
    for p in out:
        key = str(p)
        if key not in seen:
            seen.add(key); uniq.append(p)
    return uniq

def summarize_batch(name: str, path: Path) -> str:
    rows = read_csv(path)
    if not rows:
        return f'## {name}\n\nMissing batch file: `{path}`\n'
    n = len(rows)
    def cnt(field: str) -> int:
        return sum(str(r.get(field, '')).lower() == 'true' for r in rows)
    lines = [f'## {name}\n', f'- rows: {n}']
    for f in ['verify_ok','proof_ok','relation_ok','repair_ok','binding_ok','sha256_fullrelation_ok']:
        if f in rows[0]:
            lines.append(f'- {f}: {cnt(f)}/{n}')
    for f in ['proof_bytes','prove_ms','verify_ms','total_ms']:
        if f in rows[0]:
            vals = []
            for r in rows:
                try:
                    if r.get(f) not in ('', None):
                        vals.append(float(r[f]))
                except ValueError:
                    pass
            if vals:
                lines.append(f'- {f}: mean={mean(vals):.3f}, median={median(vals):.3f}, p95={pctile(vals,0.95):.3f}, p99={pctile(vals,0.99):.3f}')
    return '\n'.join(lines) + '\n'

def summarize_attack(name: str, path: Path) -> str:
    rows = read_csv(path)
    if not rows:
        return f'## {name}\n\nMissing attack file: `{path}`\n'
    honest = [r for r in rows if str(r.get('case','')).lower() == 'honest']
    attacks = [r for r in rows if str(r.get('case','')).lower() != 'honest']
    rejected = 0
    for r in attacks:
        accepted = str(r.get('verify_ok', r.get('accepted', ''))).lower() == 'true'
        if not accepted:
            rejected += 1
    return f'## {name}\n\n- honest rows: {len(honest)}\n- attack rows: {len(attacks)}\n- rejected attacks: {rejected}/{len(attacks)}\n'

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--artifact-dir', required=True)
    ap.add_argument('--project-root', default='.')
    ap.add_argument('--write-index', action='store_true')
    args = ap.parse_args()

    art = Path(args.artifact_dir)
    root = Path(args.project_root).resolve()
    main = art / 'main_results'
    v12 = main / 'jisa_v1_2_strict_lightweight'
    v20 = main / 'jisa_v2_0_sha256_authrelation_strict'
    v24 = main / 'jisa_v2_4_multi_native_extension'
    for d in (v12, v20, v24):
        d.mkdir(parents=True, exist_ok=True)

    log: list[str] = []

                            
    copy_first('v1.2 batch.csv', find_candidates(root, [
        '../fullzk-auth-lazer-system-ubuntu-v1.2.1-paper-artifact/results/jisa_paper_v1_2_strict/**/batch.csv',
        '../fullzk-auth-lazer-system-ubuntu-v1.2-paper-artifact/results/jisa_paper_v1_2_strict/**/batch.csv',
        '../fullzk-auth-lazer-system-ubuntu-v1.2.1-paper-artifact/results/jisa_paper_v1_2_strict/system_lazer_committed_mixed/batch.csv',
    ]), v12, log=log)
    copy_first('v1.2 paper_tables.md', find_candidates(root, [
        '../fullzk-auth-lazer-system-ubuntu-v1.2.1-paper-artifact/results/jisa_paper_v1_2_strict/**/paper_tables.md',
        '../fullzk-auth-lazer-system-ubuntu-v1.2-paper-artifact/results/jisa_paper_v1_2_strict/**/paper_tables.md',
        '../fullzk-auth-lazer-system-ubuntu-v1.2.1-paper-artifact/results/jisa_paper_v1_2_strict/system_lazer_committed_mixed/paper_tables.md',
    ]), v12, log=log)
    copy_first('v1.2 paper_results_summary.json', find_candidates(root, [
        '../fullzk-auth-lazer-system-ubuntu-v1.2.1-paper-artifact/results/jisa_paper_v1_2_strict/**/paper_results_summary.json',
        '../fullzk-auth-lazer-system-ubuntu-v1.2-paper-artifact/results/jisa_paper_v1_2_strict/**/paper_results_summary.json',
        '../fullzk-auth-lazer-system-ubuntu-v1.2.1-paper-artifact/results/jisa_paper_v1_2_strict/system_lazer_committed_mixed/paper_results_summary.json',
    ]), v12, log=log)
    copy_first('v1.2 adversarial_summary.csv', find_candidates(root, [
        '../fullzk-auth-lazer-system-ubuntu-v1.2.1-paper-artifact/results/jisa_paper_v1_2_strict/**/adversarial_summary.csv',
        '../fullzk-auth-lazer-system-ubuntu-v1.2-paper-artifact/results/jisa_paper_v1_2_strict/**/adversarial_summary.csv',
        '../fullzk-auth-lazer-system-ubuntu-v1.2.1-paper-artifact/results/jisa_paper_v1_2_strict/system_lazer_committed_mixed/adversarial_summary.csv',
    ]), v12, log=log)

                            
    copy_first('v2.0 batch.csv', find_candidates(root, [
        '../fullzk-auth-lazer-system-ubuntu-v2.0-sha256-authrelation/results/v2_sha256_authrelation_lazer_strict/batch.csv',
        '../fullzk-auth-lazer-system-ubuntu-v2.0-sha256-authrelation/results/v2_sha256_authrelation_lazer_strict/batch.csv',
    ]), v20, log=log)
    copy_first('v2.0 adversarial_summary.csv', find_candidates(root, [
        '../fullzk-auth-lazer-system-ubuntu-v2.0-sha256-authrelation/results/v2_sha256_authrelation_lazer_strict/adversarial_summary.csv',
        '../fullzk-auth-lazer-system-ubuntu-v2.0-sha256-authrelation/results/v2_sha256_authrelation_lazer_strict/adversarial_summary.csv',
    ]), v20, log=log)
    copy_first('v2.0 relation_coverage.md', find_candidates(root, [
        '../fullzk-auth-lazer-system-ubuntu-v2.0-sha256-authrelation/results/v2_sha256_authrelation_lazer_strict/relation_coverage.md',
        '../fullzk-auth-lazer-system-ubuntu-v2.0-sha256-authrelation/results/v2_sha256_authrelation_lazer_strict/relation_coverage.md',
    ]), v20, log=log)

                                                          
    copy_first('v2.4 multi_native_splice_summary.csv', [
        root/'results/v2_4_multi_native_lazer/multi_native_splice_summary.csv',
        art/'multi_native_splice_summary.csv',
    ] + find_candidates(root, ['results/v2_4_multi_native_lazer/multi_native_splice_summary.csv']), v24, log=log)
    copy_first('v2.4 fullrelation_result.json', [
        root/'results/v2_4_multi_native_lazer/fullrelation_smoke/fullrelation_result.json',
    ] + find_candidates(root, ['results/v2_4_multi_native_lazer/fullrelation_smoke/fullrelation_result.json']), v24, log=log)
    copy_first('v2.4 relation_coverage_v2_4.md', [
        root/'results/v2_4_multi_native_lazer/relation_coverage_v2_4.md',
    ] + find_candidates(root, ['results/v2_4_multi_native_lazer/relation_coverage_v2_4.md']), v24, log=log)
    copy_first('v2.4 combo_tamper_summary.csv', [
        root/'results/v2_4_multi_native_lazer/combo_tamper_summary.csv',
        art/'combo_tamper_summary.csv',
    ] + find_candidates(root, ['results/v2_4_multi_native_lazer/combo_tamper_summary.csv']), v24, log=log)

    (main/'integration_copy_log.txt').write_text('\n'.join(log)+'\n', encoding='utf-8')

    if args.write_index:
        (main/'JISA_INTEGRATED_EVIDENCE_INDEX.md').write_text(
'''# JISA integrated evidence index

This directory integrates the paper evidence into a single JISA-style review artifact.

## jisa_v1_2_strict_lightweight
Role: main lightweight LaZer-backed committed-offset repair result.

## jisa_v2_0_sha256_authrelation_strict
Role: SHA-256 authrelation / fullrelation-checker branch. This is extension evidence, not the lightweight main path.

## jisa_v2_4_multi_native_extension
Role: frozen extension upper bound. Multiple digest output limbs are spliced into LaZer rows 6..21.

## S0-S6 layer
Role: manifest, checksums, environment, parameter table, workload separation, serialization fuzzing, state rollback/replay, combo tamper, review README, and declarations template.
''', encoding='utf-8')

                                                      
    s0_only = art/'statistical_report.md'
    if s0_only.exists():
        shutil.copy2(s0_only, art/'jisa_s0_s6_robustness_statistical_report.md')

    md: list[str] = []
    md.append('# JISA integrated statistical report\n')
    md.append('This report integrates the main paper evidence and the S0-S6 robustness/reproducibility layer.\n')
    md.append('## Evidence layers\n')
    md.append('- `main_results/jisa_v1_2_strict_lightweight`: lightweight LaZer-backed committed-offset repair main path.')
    md.append('- `main_results/jisa_v2_0_sha256_authrelation_strict`: SHA-256 authrelation/fullrelation-checker branch.')
    md.append('- `main_results/jisa_v2_4_multi_native_extension`: frozen multi-native digest output-limb splice extension.')
    md.append('- S0-S6 files: workload separation, serialization fuzzing, state rollback/replay, combo tamper, manifest, checksums and environment.\n')

    md.append(summarize_batch('jisa_v1_2_strict_lightweight: main result', v12/'batch.csv'))
    md.append(summarize_attack('jisa_v1_2_strict_lightweight: adversarial result', v12/'adversarial_summary.csv'))
    md.append(summarize_batch('jisa_v2_0_sha256_authrelation_strict: coverage branch', v20/'batch.csv'))
    md.append(summarize_attack('jisa_v2_0_sha256_authrelation_strict: adversarial result', v20/'adversarial_summary.csv'))

    md.append('## jisa_v2_4_multi_native_extension: frozen extension upper bound\n')
    v24rows = read_csv(v24/'multi_native_splice_summary.csv')
    if v24rows:
        honest = [r for r in v24rows if r.get('case') == 'honest']
        attacks = [r for r in v24rows if r.get('case') != 'honest']
        rejected = sum(str(r.get('verify_ok','')).lower() != 'true' for r in attacks)
        md.append(f'- rows: {len(v24rows)}')
        md.append(f'- honest rows: {len(honest)}')
        md.append(f'- attack rows: {len(attacks)}')
        md.append(f'- rejected attacks: {rejected}/{len(attacks)}')
        if honest:
            h = honest[0]
            for f in ['proof_bytes','prove_ms','verify_ms','total_ms']:
                if f in h:
                    md.append(f'- honest {f}: {h[f]}')
    else:
        md.append('- Missing v2.4 multi_native_splice_summary.csv')

    md.append('\n## S0-S6 robustness summary\n')
    for label, fname in [
        ('workload profile rows', 'workload_profiles_summary.csv'),
        ('serialization fuzz rows', 'serialization_fuzz_summary.csv'),
        ('state rollback rows', 'state_rollback_summary.csv'),
        ('combo tamper rows', 'combo_tamper_summary.csv'),
        ('multi-native splice rows', 'multi_native_splice_summary.csv'),
    ]:
        rows = read_csv(art/fname)
        md.append(f'- {label}: {len(rows)}')

    md.append('\n## Interpretation boundary\n')
    md.append('The v1.2 strict result is the main lightweight result. v2.0 and v2.4 are extension branches for relation coverage and coverage-cost trade-off. The v2.4 branch binds multiple SHA-256 digest output limbs into LaZer rows 6..21, while full SHA-256 compression/preimage-to-digest constraints remain in the fullrelation checker/IR path. This report supports a system-level research artifact; it does not claim a production UC-secure anonymous-authentication library.')

    (art/'statistical_report.md').write_text('\n'.join(md)+'\n', encoding='utf-8')

                                                                      
    manifest = art/'artifact_manifest.json'
    if manifest.exists():
        try:
            data = json.loads(manifest.read_text(encoding='utf-8'))
        except Exception:
            data = {}
        data.update({
            'artifact_version': 'jisa_s0_s6_lazer_integrated_v1',
            'jisa_artifact_name': art.name,
            'integrated_main_results': [
                'jisa_v1_2_strict_lightweight',
                'jisa_v2_0_sha256_authrelation_strict',
                'jisa_v2_4_multi_native_extension',
            ],
        })
        manifest.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding='utf-8')

    print(f'[OK] wrote integrated report: {art/"statistical_report.md"}')
    print(f'[OK] wrote integration log: {main/"integration_copy_log.txt"}')

if __name__ == '__main__':
    main()
