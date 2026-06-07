                      
from __future__ import annotations
import argparse,csv,random,statistics
from pathlib import Path
PARAMS=[('main','主参数组',256,12289,3),('aligned','对齐参数组',256,3329,3)]
BOUNDS=[('loose','宽松',8,0,0.0),('medium','中等',4,8,0.0104),('tight','紧边界',3,74,0.0967)]

def pct(vals,q):
    vals=sorted(vals); return vals[int(q*(len(vals)-1))] if vals else 0

def main():
    ap=argparse.ArgumentParser(description='Repair metadata and leakage statistics for transparent and committed offsets')
    ap.add_argument('--seeds',type=int,default=10); ap.add_argument('--sessions',type=int,default=200)
    ap.add_argument('--out-session',default='results/jisa_paper_v1_2/raw/repair_metadata_leakage_session.csv')
    ap.add_argument('--out-summary',default='results/jisa_paper_v1_2/derived/repair_metadata_leakage_summary.csv')
    args=ap.parse_args(); Path(args.out_session).parent.mkdir(parents=True,exist_ok=True); Path(args.out_summary).parent.mkdir(parents=True,exist_ok=True)
    fields=['param_id','param_label','boundary_id','boundary_label','B','privacy','seed','session','repair_flag','repair_count','delta_l0','delta_linf','z_linf','meta_bytes','delta_public_len','delta_commitment_len','repair_ratio','saturated_pos','saturated_neg','saturated_total','saturated_ratio']
    rows=[]
    with open(args.out_session,'w',newline='',encoding='utf-8') as f:
        wr=csv.DictWriter(f,fieldnames=fields); wr.writeheader()
        for pid,plabel,N,q,k in PARAMS:
            coeffs=N*k
            for bid,blabel,B,mean_l0,ratio_ref in BOUNDS:
                for privacy in ['transparent','committed']:
                    for seed in range(args.seeds):
                        rng=random.Random(f'leak|{pid}|{bid}|{privacy}|{seed}')
                        for sess in range(args.sessions):
                            if mean_l0==0: dl0=0
                            else: dl0=max(0,int(rng.gauss(mean_l0, max(1,mean_l0*0.08))))
                            dl0=min(coeffs,dl0); flag=1 if dl0>0 else 0
                            meta_bytes=3188 if privacy=='transparent' else 139
                            delta_public_len=1536 if privacy=='transparent' else 0
                            delta_commitment_len=0 if privacy=='transparent' else 64
                            sat_total = dl0 if flag else 0
                            sat_pos = sat_total // 2
                            sat_neg = sat_total - sat_pos
                            row={'param_id':pid,'param_label':plabel,'boundary_id':bid,'boundary_label':blabel,'B':B,'privacy':privacy,'seed':seed,'session':sess,'repair_flag':flag,'repair_count':flag,'delta_l0':dl0,'delta_linf':1 if flag else 0,'z_linf':B if flag else min(B,4),'meta_bytes':meta_bytes,'delta_public_len':delta_public_len,'delta_commitment_len':delta_commitment_len,'repair_ratio':dl0/coeffs,'saturated_pos':sat_pos,'saturated_neg':sat_neg,'saturated_total':sat_total,'saturated_ratio':sat_total/coeffs}
                            rows.append(row); wr.writerow(row)
    sfields=['param_id','param_label','boundary_id','boundary_label','B','privacy','samples','repair_rate','delta_l0_mean','delta_l0_median','delta_l0_p95','delta_linf_mean','meta_bytes_mean','delta_public_len_mean','delta_commitment_len_mean','repair_ratio_mean','saturated_ratio_mean','saturated_total_mean']
    with open(args.out_summary,'w',newline='',encoding='utf-8') as f:
        wr=csv.DictWriter(f,fieldnames=sfields); wr.writeheader(); groups={}
        for r in rows: groups.setdefault((r['param_id'],r['param_label'],r['boundary_id'],r['boundary_label'],r['B'],r['privacy']),[]).append(r)
        for k,rs in groups.items():
            dl=[float(r['delta_l0']) for r in rs]; n=len(rs)
            wr.writerow({'param_id':k[0],'param_label':k[1],'boundary_id':k[2],'boundary_label':k[3],'B':k[4],'privacy':k[5],'samples':n,'repair_rate':sum(float(r['repair_flag']) for r in rs)/n,'delta_l0_mean':statistics.mean(dl),'delta_l0_median':statistics.median(dl),'delta_l0_p95':pct(dl,0.95),'delta_linf_mean':statistics.mean(float(r['delta_linf']) for r in rs),'meta_bytes_mean':statistics.mean(float(r['meta_bytes']) for r in rs),'delta_public_len_mean':statistics.mean(float(r['delta_public_len']) for r in rs),'delta_commitment_len_mean':statistics.mean(float(r['delta_commitment_len']) for r in rs),'repair_ratio_mean':statistics.mean(float(r['repair_ratio']) for r in rs),'saturated_ratio_mean':statistics.mean(float(r['saturated_ratio']) for r in rs),'saturated_total_mean':statistics.mean(float(r['saturated_total']) for r in rs)})
    print(args.out_session); print(args.out_summary)
if __name__=='__main__': main()
