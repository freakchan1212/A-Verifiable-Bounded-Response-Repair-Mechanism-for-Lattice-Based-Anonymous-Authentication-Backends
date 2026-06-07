                      
from __future__ import annotations
import argparse,csv,json,statistics
from pathlib import Path

def rows(p):
    p=Path(p)
    if not p.exists(): return []
    with open(p,newline='',encoding='utf-8') as f: return list(csv.DictReader(f))

def fnum(x, default=0.0):
    try:
        if x in (None,'','inf'): return default
        return float(x)
    except Exception: return default

def bval(x): return str(x).lower() in {'true','1','yes'}

def pct(vals,q):
    vals=sorted(vals); return vals[int(q*(len(vals)-1))] if vals else 0.0

def stat(vals):
    return {'mean':statistics.mean(vals) if vals else 0.0,'median':statistics.median(vals) if vals else 0.0,'p95':pct(vals,0.95),'p99':pct(vals,0.99)}

def summarize_system(batch_rows):
    out={'rows':len(batch_rows)}
    if not batch_rows: return out
    for k in ['verify_ok','proof_ok','relation_ok','repair_ok','binding_ok']:
        out[k]=f"{sum(bval(r.get(k)) for r in batch_rows)}/{len(batch_rows)}"
    out['repair_sessions']=f"{sum(int(fnum(r.get('repair_flag'))) for r in batch_rows)}/{len(batch_rows)}"
    for k in ['proof_bytes','prove_ms','verify_ms','total_ms','delta_l0','meta_bytes','repair_flag']:
        vals=[fnum(r.get(k)) for r in batch_rows if k in r]
        if vals: out[k]=stat(vals)
    return out

def main():
    ap=argparse.ArgumentParser(description='Generate V12/V13 aligned paper tables and JSON summary')
    ap.add_argument('--root',default='results/jisa_paper_v1_2')
    ap.add_argument('--system-dir',default='')
    ap.add_argument('--out-md',default='results/jisa_paper_v1_2/paper_tables.md')
    ap.add_argument('--out-json',default='results/jisa_paper_v1_2/paper_results_summary.json')
    args=ap.parse_args(); root=Path(args.root); der=root/'derived'; raw=root/'raw'
    hit=rows(raw/'boundary_hit_probability.csv')
    abort=rows(der/'abort_retry_summary.csv')
    clip=rows(der/'local_clipping_baseline_summary.csv')
    bind=rows(der/'binding_ablation_summary.csv')
    leak=rows(der/'repair_metadata_leakage_summary.csv')
    sysdir=Path(args.system_dir) if args.system_dir else root/'system_lazer_committed_mixed'
    sysbatch=rows(sysdir/'batch.csv'); adv=rows(sysdir/'adversarial_summary.csv')
    anon=rows(sysdir/'anonymity_game.csv'); unlink=rows(sysdir/'unlinkability_game.csv'); zk=rows(sysdir/'zk_distinguish_sanity.csv')
    summary={'boundary_hit_rows':len(hit),'abort_rows':len(abort),'clip_rows':len(clip),'binding_rows':len(bind),'leakage_rows':len(leak),'system':summarize_system(sysbatch)}
    summary['adversarial']={'rows':len(adv),'accepted':[r.get('case') for r in adv if bval(r.get('verify_ok'))],'rejected':[r.get('case') for r in adv if not bval(r.get('verify_ok'))]}
    for key,rs in [('anonymity',anon),('unlinkability',unlink),('zk_distinguish_sanity',zk)]:
        summary[key]={'rows':len(rs),'accuracy':(sum(bval(r.get('correct')) for r in rs)/len(rs) if rs else None)}
    Path(args.out_json).parent.mkdir(parents=True,exist_ok=True); Path(args.out_json).write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    md=[]; md.append('# V12/V13-aligned paper tables\n\n')
    md.append('## Table A. Boundary hit probability\n\n| Param | Boundary | B | Trials | Hit rate | 95% CI | Expected attempts |\n|---|---:|---:|---:|---:|---:|---:|\n')
                                                  
    groups={}
    for r in hit:
        k=(r['param_label'],r['boundary_label'],r['B']); groups.setdefault(k,[]).append(r)
    for k,rs in groups.items():
        trials=sum(int(r['trials']) for r in rs); hits=sum(int(r['hits']) for r in rs); rate=hits/trials if trials else 0
        lo=statistics.mean(fnum(r['ci95_low']) for r in rs); hi=statistics.mean(fnum(r['ci95_high']) for r in rs)
        exp='未命中' if hits==0 else f'{1/rate:.2f}'
        md.append(f'| {k[0]} | {k[1]} | {k[2]} | {trials} | {rate:.5f} | [{lo:.5f}, {hi:.5f}] | {exp} |\n')
    md.append('\n## Table B. Tight-bound repair versus abort/retry\n\n| Param | Method | Budget | Samples | Success rate | Attempts mean | Total ms mean / p95 |\n|---|---|---:|---:|---:|---:|---:|\n')
    for r in abort:
        if r.get('boundary_id')=='tight' and (r.get('method')=='deterministic_repair' or r.get('budget')=='2000'):
            md.append(f"| {r['param_label']} | {r['method']} | {r['budget']} | {r['samples']} | {fnum(r['success_rate']):.3f} | {fnum(r['attempts_mean']):.2f} | {fnum(r['total_ms_mean']):.2f} / {fnum(r['total_ms_p95']):.2f} |\n")
    md.append('\n## Table C. Local clipping semantic gap\n\n| Param | Boundary | Repair rate | AlgCheck(z) | AlgCheck(z+Delta) | Full repair | Semantic gap |\n|---|---|---:|---:|---:|---:|---:|\n')
    for r in clip:
        md.append(f"| {r['param_label']} | {r['boundary_label']} | {fnum(r['repair_rate']):.3f} | {fnum(r['alg_z_ok_rate']):.3f} | {fnum(r['alg_recovered_ok_rate']):.3f} | {fnum(r['full_repair_ok_rate']):.3f} | {fnum(r['semantic_gap_rate']):.3f} |\n")
    md.append('\n## Table D. Binding ablation\n\n| Attack | Full verifier | No context check | No refresh check | No binding |\n|---|---:|---:|---:|---:|\n')
    for r in bind:
        md.append(f"| {r['attack_label']} | {fnum(r['full_accept_rate']):.3f} | {fnum(r['no_context_accept_rate']):.3f} | {fnum(r['no_refresh_accept_rate']):.3f} | {fnum(r['no_binding_accept_rate']):.3f} |\n")
    md.append('\n## Table E. Leakage and offset visibility\n\n| Param | Boundary | Privacy | Repair rate | Delta L0 mean | Metadata bytes | Delta public len | Delta commitment len |\n|---|---|---|---:|---:|---:|---:|---:|\n')
    for r in leak:
        if r.get('boundary_id') in {'medium','tight'}:
            md.append(f"| {r['param_label']} | {r['boundary_label']} | {r['privacy']} | {fnum(r['repair_rate']):.3f} | {fnum(r['delta_l0_mean']):.2f} | {fnum(r['meta_bytes_mean']):.0f} | {fnum(r['delta_public_len_mean']):.0f} | {fnum(r['delta_commitment_len_mean']):.0f} |\n")
    b=summary['system']; md.append('\n## Table F. LaZer-backed full-system acceptance and cost\n\n| Metric | Value |\n|---|---:|\n')
    md.append(f"| Sessions | {b.get('rows',0)} |\n")
    for k in ['verify_ok','proof_ok','relation_ok','repair_ok','binding_ok','repair_sessions']:
        if k in b: md.append(f'| {k} | {b[k]} |\n')
    for k,label in [('proof_bytes','Proof size (B)'),('prove_ms','Prove time (ms)'),('verify_ms','Verify time (ms)'),('total_ms','Total time (ms)')]:
        if k in b:
            v=b[k]; md.append(f"| {label}, mean / median / p95 / p99 | {v['mean']:.3f} / {v['median']:.3f} / {v['p95']:.3f} / {v['p99']:.3f} |\n")
    md.append('\n## Table G. Adversarial-path rejection\n\n| Case | verify_ok | failed checks |\n|---|---:|---|\n')
    for r in adv: md.append(f"| {r.get('case')} | {r.get('verify_ok')} | {r.get('failed_checks','')} |\n")
    md.append('\n## Table H. Engineering sanity checks\n\n| Check | Rows | Accuracy | Note |\n|---|---:|---:|---|\n')
    for key,note in [('anonymity','not a formal anonymity proof'),('unlinkability','not a formal unlinkability proof'),('zk_distinguish_sanity','not a cryptographic ZK proof')]:
        s=summary[key]; acc='n/a' if s['accuracy'] is None else f"{s['accuracy']:.4f}"
        md.append(f"| {key} | {s['rows']} | {acc} | {note} |\n")
    md.append('\n## Boundary statement\n\nLaZer proves the core lattice relation and committed-offset repair relation in this artifact. Credential, hash, nullifier and refresh bindings remain bridge-level verifier checks unless explicitly moved into an extended relation.\n')
    Path(args.out_md).write_text(''.join(md),encoding='utf-8'); print(args.out_md); print(args.out_json)
if __name__=='__main__': main()
