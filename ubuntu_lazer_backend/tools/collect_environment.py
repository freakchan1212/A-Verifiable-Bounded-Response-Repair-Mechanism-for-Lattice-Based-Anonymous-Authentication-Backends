from __future__ import annotations
import argparse, json, os, platform, shutil, subprocess, sys
from pathlib import Path

def run(cmd):
    try:
        return subprocess.check_output(cmd, stderr=subprocess.STDOUT, text=True, timeout=10).strip()
    except Exception as e:
        return f"unavailable: {e}"

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--out', required=True)
    ap.add_argument('--lazer-root', default='third_party/lazer')
    args=ap.parse_args()
    root=Path(args.lazer_root)
    data={
        'python': sys.version,
        'platform': platform.platform(),
        'machine': platform.machine(),
        'processor': platform.processor(),
        'cpu_count': os.cpu_count(),
        'gcc': run(['gcc','--version']).splitlines()[0] if shutil.which('gcc') else 'missing',
        'g++': run(['g++','--version']).splitlines()[0] if shutil.which('g++') else 'missing',
        'git_version': run(['git','--version']) if shutil.which('git') else 'missing',
        'project_git_commit': run(['git','rev-parse','HEAD']) if Path('.git').exists() else 'not a git repository',
        'lazer_root': str(root),
        'lazer_git_commit': run(['git','-C',str(root),'rev-parse','HEAD']) if (root/'.git').exists() else 'not available',
        'env': {k: os.environ.get(k,'') for k in ['LAZER_ROOT','HEXL_LIB','LD_LIBRARY_PATH','CC','CXX']},
    }
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out).write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding='utf-8')
    print(args.out)
if __name__=='__main__': main()
