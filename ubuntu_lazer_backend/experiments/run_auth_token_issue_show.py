from __future__ import annotations

import argparse
import csv
from pathlib import Path

from fullzk.auth_token import issue_token, verify_token

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-csv", default="results/v2_1_auth_token_issue_show.csv")
    args = ap.parse_args()
    issuer_pk = "issuer-pk-demo"
    scope = "edge-scope-demo"
    member_secret = "member-secret-demo"
    token_randomness = "token-randomness-demo"
    token = issue_token(member_secret, token_randomness, issuer_pk, scope)
    cases = [
        ("honest", member_secret, token_randomness, issuer_pk, scope, token.membership_commitment, token.membership_tag, True),
        ("wrong_member_secret", "bad-member-secret", token_randomness, issuer_pk, scope, token.membership_commitment, token.membership_tag, False),
        ("wrong_token_randomness", member_secret, "bad-randomness", issuer_pk, scope, token.membership_commitment, token.membership_tag, False),
        ("wrong_issuer_scope", member_secret, token_randomness, issuer_pk, "wrong-scope", token.membership_commitment, token.membership_tag, False),
        ("tamper_commitment", member_secret, token_randomness, issuer_pk, scope, "00" + token.membership_commitment[2:], token.membership_tag, False),
        ("tamper_tag", member_secret, token_randomness, issuer_pk, scope, token.membership_commitment, "00" + token.membership_tag[2:], False),
    ]
    rows = []
    for case, ms, tr, ipk, sc, c, t, expected in cases:
        ok = verify_token(ms, tr, ipk, sc, c, t)
        rows.append({"case": case, "issue_show_ok": ok, "expected": expected, "pass": ok == expected})
    Path(args.out_csv).parent.mkdir(parents=True, exist_ok=True)
    with open(args.out_csv, "w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        wr.writeheader(); wr.writerows(rows)
    failures = [r for r in rows if not r["pass"]]
    print(args.out_csv)
    if failures:
        print("unexpected:", failures)
        raise SystemExit(2)

if __name__ == "__main__":
    main()
