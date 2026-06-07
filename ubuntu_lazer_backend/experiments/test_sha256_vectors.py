from __future__ import annotations

import hashlib
from fullzk.circuits.sha256_bits import sha256_hexdigest, sha256_relation_check

VECTORS = [b"", b"abc", b"hello", b"The quick brown fox jumps over the lazy dog"]

def main() -> None:
    failures = 0
    for msg in VECTORS:
        expected = hashlib.sha256(msg).hexdigest()
        got = sha256_hexdigest(msg)
        ok, stats, got2 = sha256_relation_check(msg, expected)
        print(repr(msg), expected, got, "ok=", ok, "blocks=", stats.blocks, "rounds=", stats.rounds)
        failures += 0 if (got == expected and got2 == expected and ok) else 1
    raise SystemExit(1 if failures else 0)

if __name__ == "__main__":
    main()
