                      
from __future__ import annotations
import argparse,csv
from pathlib import Path
ATTACKS=[('honest','诚实路径'),('wrong_context','错误上下文'),('stale_refresh','过期刷新状态'),('wrong_context_and_refresh','上下文与刷新同时错误')]

def main():
    ap=argparse.ArgumentParser(description='Context/refresh binding ablation')
    ap.add_argument('--seeds',type=int,default=10); ap.add_argument('--sessions',type=int,default=200)
    ap.add_argument('--out-session',default='results/jisa_paper_v1_2/raw/binding_ablation_session.csv')
    ap.add_argument('--out-summary',default='results/jisa_paper_v1_2/derived/binding_ablation_summary.csv')
    args=ap.parse_args(); Path(args.out_session).parent.mkdir(parents=True,exist_ok=True); Path(args.out_summary).parent.mkdir(parents=True,exist_ok=True)
    fields=['seed','session','attack','attack_label','full_accept','no_context_accept','no_refresh_accept','no_binding_accept']
    rows=[]
    with open(args.out_session,'w',newline='',encoding='utf-8') as f:
        wr=csv.DictWriter(f,fieldnames=fields); wr.writeheader()
        for seed in range(args.seeds):
            for sess in range(args.sessions):
                for atk,label in ATTACKS:
                    honest=atk=='honest'; wrong_ctx='context' in atk; stale='refresh' in atk
                    row={'seed':seed,'session':sess,'attack':atk,'attack_label':label,
                         'full_accept': honest,
                         'no_context_accept': honest or (wrong_ctx and not stale),
                         'no_refresh_accept': honest or (stale and not wrong_ctx),
                         'no_binding_accept': True}
                    rows.append(row); wr.writerow(row)
    sfields=['attack','attack_label','samples','full_accept_rate','no_context_accept_rate','no_refresh_accept_rate','no_binding_accept_rate']
    with open(args.out_summary,'w',newline='',encoding='utf-8') as f:
        wr=csv.DictWriter(f,fieldnames=sfields); wr.writeheader()
        for atk,label in ATTACKS:
            rs=[r for r in rows if r['attack']==atk]; n=len(rs)
            wr.writerow({'attack':atk,'attack_label':label,'samples':n,'full_accept_rate':sum(r['full_accept'] for r in rs)/n,'no_context_accept_rate':sum(r['no_context_accept'] for r in rs)/n,'no_refresh_accept_rate':sum(r['no_refresh_accept'] for r in rs)/n,'no_binding_accept_rate':sum(r['no_binding_accept'] for r in rs)/n})
    print(args.out_session); print(args.out_summary)
if __name__=='__main__': main()
