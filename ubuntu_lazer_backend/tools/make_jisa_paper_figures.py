                      
from __future__ import annotations
import argparse,csv,json,statistics,sys
from pathlib import Path
try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
except ModuleNotFoundError as e:
    raise SystemExit('Missing matplotlib. Install it with: python3 -m pip install matplotlib') from e

def rows(p):
    p=Path(p)
    if not p.exists(): return []
    with open(p,newline='',encoding='utf-8') as f: return list(csv.DictReader(f))

def fnum(x, default=0.0):
    try:
        if x in (None,'','inf'): return default
        return float(x)
    except Exception: return default

def savefig(outdir: Path, name: str):
    outdir.mkdir(parents=True,exist_ok=True)
    plt.tight_layout()
    plt.savefig(outdir/(name+'.png'), dpi=300)
    plt.savefig(outdir/(name+'.pdf'))
    plt.close()

def fig_hit_probability(root: Path, out: Path):
    data=rows(root/'raw/boundary_hit_probability.csv')
    groups={}
    for r in data:
        k=(r['param_label'],r['boundary_label']); groups.setdefault(k,[0,0]); groups[k][0]+=int(r['hits']); groups[k][1]+=int(r['trials'])
    labels=[]; vals=[]
    for param in ['主参数组','对齐参数组']:
        for bound in ['宽松','中等','紧边界']:
            labels.append({'主参数组':'Main','对齐参数组':'Aligned'}.get(param,param)+'\n'+{'宽松':'Loose','中等':'Medium','紧边界':'Tight'}.get(bound,bound)); h,t=groups.get((param,bound),[0,1]); vals.append(h/t)
    plt.figure(figsize=(8.6,4.8)); plt.bar(range(len(vals)), vals)
    plt.xticks(range(len(vals)), labels, rotation=0); plt.ylabel('Single-try hit probability'); plt.ylim(0,1.05)
    plt.title('Boundary hit probability under representative parameter groups')
    savefig(out,'fig_7_1_boundary_hit_probability')

def fig_abort_retry(root: Path, out: Path):
    data=rows(root/'derived/abort_retry_summary.csv')
    vals=[]; labels=[]
    for param in ['主参数组','对齐参数组']:
        for method,budget in [('deterministic_repair','-1'),('abort_retry','2000')]:
            rs=[r for r in data if r.get('param_label')==param and r.get('boundary_id')=='tight' and r.get('method')==method and str(r.get('budget'))==budget]
            if rs:
                labels.append({'主参数组':'Main','对齐参数组':'Aligned'}.get(param,param)+'\n'+('repair' if method=='deterministic_repair' else 'abort-2000'))
                vals.append(fnum(rs[0]['total_ms_mean']))
    plt.figure(figsize=(8.4,4.8)); plt.bar(range(len(vals)), vals)
    plt.xticks(range(len(vals)), labels); plt.ylabel('Mean total latency (ms)')
    plt.title('Tight-bound repair path versus high-budget abort/retry')
    savefig(out,'fig_7_2_abort_retry_latency')

def fig_local_clipping(root: Path, out: Path):
    data=rows(root/'derived/local_clipping_baseline_summary.csv')
    data=[r for r in data if r.get('boundary_id')=='tight']
    labels=[{'主参数组':'Main','对齐参数组':'Aligned'}.get(r['param_label'], r['param_label']) for r in data]
    metrics=['alg_z_ok_rate','alg_recovered_ok_rate','full_repair_ok_rate']
    x=range(len(labels)); width=0.22
    plt.figure(figsize=(8.4,4.8))
    for j,m in enumerate(metrics):
        plt.bar([i+(j-1)*width for i in x],[fnum(r[m]) for r in data],width,label=m.replace('_rate',''))
    plt.xticks(list(x), labels); plt.ylabel('Acceptance rate'); plt.ylim(0,1.05); plt.legend()
    plt.title('Local clipping semantic gap under tight boundary')
    savefig(out,'fig_7_3_local_clipping_semantic_gap')

def fig_binding_ablation(root: Path, out: Path):
    data=rows(root/'derived/binding_ablation_summary.csv')
    labels=[{'诚实路径':'Honest','错误上下文':'Wrong ctx','过期刷新状态':'Stale refresh','上下文与刷新同时错误':'Both wrong'}.get(r['attack_label'], r['attack_label']) for r in data]
    metrics=['full_accept_rate','no_context_accept_rate','no_refresh_accept_rate','no_binding_accept_rate']
    x=range(len(labels)); width=0.18
    plt.figure(figsize=(10,5.0))
    for j,m in enumerate(metrics):
        plt.bar([i+(j-1.5)*width for i in x],[fnum(r[m]) for r in data],width,label=m.replace('_accept_rate',''))
    plt.xticks(list(x), labels, rotation=15, ha='right'); plt.ylabel('Acceptance rate'); plt.ylim(0,1.05); plt.legend(fontsize=8)
    plt.title('Context and refresh binding ablation')
    savefig(out,'fig_7_4_binding_ablation')

def fig_leakage(root: Path, out: Path):
    data=rows(root/'derived/repair_metadata_leakage_summary.csv')
    data=[r for r in data if r.get('privacy')=='committed']
    labels=[]; dl0=[]; rate=[]
    for param in ['主参数组','对齐参数组']:
        for bound in ['宽松','中等','紧边界']:
            rs=[r for r in data if r['param_label']==param and r['boundary_label']==bound]
            if rs:
                labels.append({'主参数组':'Main','对齐参数组':'Aligned'}.get(param,param)+'\n'+{'宽松':'Loose','中等':'Medium','紧边界':'Tight'}.get(bound,bound)); dl0.append(fnum(rs[0]['delta_l0_mean'])); rate.append(fnum(rs[0]['repair_rate']))
    plt.figure(figsize=(9.2,4.8)); plt.bar(range(len(dl0)), dl0)
    plt.xticks(range(len(labels)), labels); plt.ylabel('Mean Delta L0'); plt.title('Committed-offset repair strength by boundary')
    savefig(out,'fig_7_5_repair_strength_committed')
    plt.figure(figsize=(9.2,4.8)); plt.bar(range(len(rate)), rate)
    plt.xticks(range(len(labels)), labels); plt.ylabel('Repair rate'); plt.ylim(0,1.05); plt.title('Repair triggering by boundary')
    savefig(out,'fig_7_6_repair_trigger_rate')

def fig_system_cost(root: Path, out: Path):
    summ_path=root/'paper_results_summary.json'
    if not summ_path.exists(): return
    s=json.loads(summ_path.read_text(encoding='utf-8'))
    b=s.get('system',{}) or s.get('batch',{})
    labels=['proof_bytes','prove_ms','verify_ms','total_ms']; vals=[]
    for k in labels:
        v=b.get(k,{})
        vals.append(float(v.get('mean',0.0)) if isinstance(v,dict) else 0.0)
    plt.figure(figsize=(7.4,4.8)); plt.bar(range(len(vals)), vals)
    plt.xticks(range(len(vals)), ['Proof bytes','Prove ms','Verify ms','Total ms']); plt.ylabel('Mean value')
    plt.title('LaZer-backed full-system cost overview')
    savefig(out,'fig_7_7_lazer_system_cost')

def fig_adversarial(root: Path, out: Path):
    data=rows(root/'system_lazer_committed_mixed/adversarial_summary.csv') or rows(root/'adversarial_summary.csv')
    if not data: return
    labels=[r['case'].replace('tamper_','') for r in data]
    vals=[1.0 if str(r.get('verify_ok')).lower()=='true' else 0.0 for r in data]
    plt.figure(figsize=(10,4.8)); plt.bar(range(len(vals)), vals)
    plt.xticks(range(len(vals)), labels, rotation=35, ha='right'); plt.yticks([0,1],['rejected','accepted']); plt.ylim(-0.05,1.05)
    plt.title('Adversarial-path acceptance result')
    savefig(out,'fig_7_8_adversarial_rejection')

def fig_sanity(root: Path, out: Path):
    summ_path=root/'paper_results_summary.json'
    if not summ_path.exists(): return
    s=json.loads(summ_path.read_text(encoding='utf-8'))
    labels=[]; vals=[]
    for k in ['anonymity','unlinkability','zk_distinguish_sanity']:
        if k in s and s[k].get('accuracy') is not None:
            labels.append(k); vals.append(float(s[k]['accuracy']))
    if not vals: return
    plt.figure(figsize=(7.2,4.8)); plt.bar(range(len(vals)), vals); plt.axhline(0.5, linestyle='--')
    plt.xticks(range(len(vals)), labels, rotation=15, ha='right'); plt.ylabel('Classifier/guess accuracy'); plt.ylim(0,1)
    plt.title('Engineering sanity checks near random guessing')
    savefig(out,'fig_7_9_sanity_checks')

def main():
    ap=argparse.ArgumentParser(description='Generate all paper-ready figures from unified artifact CSVs')
    ap.add_argument('--root',default='results/jisa_paper_v1_2')
    ap.add_argument('--out-dir',default='results/jisa_paper_v1_2/figures')
    args=ap.parse_args(); root=Path(args.root); out=Path(args.out_dir)
    for fn in [fig_hit_probability,fig_abort_retry,fig_local_clipping,fig_binding_ablation,fig_leakage,fig_system_cost,fig_adversarial,fig_sanity]:
        fn(root,out)
    manifest=out/'figure_manifest.md'
    files=sorted(p.name for p in out.glob('*.png'))
    manifest.write_text('# Figure manifest\n\n'+'\n'.join(f'- `{x}`' for x in files)+'\n',encoding='utf-8')
    print(out); print(manifest)
if __name__=='__main__': main()
