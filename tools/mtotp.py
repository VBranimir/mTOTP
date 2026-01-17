#!/usr/bin/env python3

from __future__ import annotations

import argparse
import datetime as dt
import sys
from typing import List, Optional, Tuple


def parse_key_digits(key: str) -> List[int]:
    s = key.strip()
    if len(s) != 10 or not s.isdigit():
        raise ValueError("Key must be exactly 10 digits (0-9).")
    return [int(ch) for ch in s]


def parse_at(arg: Optional[str]) -> dt.datetime:
    if not arg:
        return dt.datetime.now()
    s = arg.strip()

    # Accept YYYYMMDDHHMM
    if s.isdigit() and len(s) == 12:
        y = int(s[0:4])
        mo = int(s[4:6])
        d = int(s[6:8])
        h = int(s[8:10])
        mi = int(s[10:12])
        return dt.datetime(y, mo, d, h, mi)

    # Accept ISO-ish: 2026-01-17T12:34 or 2026-01-17 12:34
    try:
        s2 = s.replace(" ", "T")
        return dt.datetime.fromisoformat(s2)
    except Exception as e:
        raise ValueError("Invalid --at. Use YYYYMMDDHHMM or ISO like 2026-01-17T12:34.") from e


def time_to_10digits(ts: dt.datetime) -> List[int]:
    return [int(ch) for ch in ts.strftime("%y%m%d%H%M")]


def derive_sbox(key_digits: List[int]) -> List[int]:
    seen = set()
    perm: List[int] = []
    for d in key_digits:
        if d not in seen:
            perm.append(d)
            seen.add(d)
    for d in range(10):
        if d not in seen:
            perm.append(d)
    if len(perm) != 10:
        raise RuntimeError("S-box derivation failed.")
    return perm  # S(x) = perm[x]


def apply_sbox(c: List[int], sbox: List[int]) -> List[int]:
    return [sbox[x] for x in c]


def diffuse(c: List[int]) -> List[int]:
    a = c[9]
    out = c[:]
    for i in range(10):
        out[i] = (out[i] + a) % 10
        a = out[i]
    return out


def fold_to_6(c: List[int]) -> str:
    o1 = (c[0] + c[5]) % 10
    o2 = (c[1] + c[6]) % 10
    o3 = (c[2] + c[7]) % 10
    o4 = (c[3] + c[8]) % 10
    o5 = (c[4] + c[9]) % 10
    o6 = sum(c) % 10
    return f"{o1}{o2}{o3}{o4}{o5}{o6}"


def generate(ts: dt.datetime, key_digits: List[int], show_steps: bool) -> Tuple[str, Optional[str]]:
    t = time_to_10digits(ts)
    c0 = [(t[i] + key_digits[i]) % 10 for i in range(10)]
    sbox = derive_sbox(key_digits)
    c1 = apply_sbox(c0, sbox)
    c2 = diffuse(c1)
    code = fold_to_6(c2)

    if not show_steps:
        return code, None

    lines = []
    lines.append(f"time YYMMDDHHMM: {''.join(map(str, t))}")
    lines.append(f"key digits     : {''.join(map(str, key_digits))}")
    lines.append(f"combine (mod10): {''.join(map(str, c0))}")
    lines.append(f"sbox perm      : {''.join(map(str, sbox))}   (S(x)=perm[x])")
    lines.append(f"after sbox     : {''.join(map(str, c1))}")
    lines.append(f"after diffuse  : {''.join(map(str, c2))}")
    lines.append(f"otp(6)         : {code}")
    return code, "\n".join(lines)


def cmd_gen(args: argparse.Namespace) -> int:
    key_digits = parse_key_digits(args.key)
    ts = parse_at(args.at)
    code, steps = generate(ts, key_digits, show_steps=args.show_steps)

    if args.eval is not None:
        assumed = args.eval.strip()
        if not (assumed.isdigit() and len(assumed) == 6):
            print("Assumed code for --eval must be exactly 6 digits.", file=sys.stderr)
            return 2
        ok = (assumed == code)
        print("true" if ok else "false")
        if args.show_steps:
            print()
            print(steps or "")
        return 0 if ok else 1

    print(code)
    if args.show_steps:
        print()
        print(steps or "")
    return 0


def cmd_verify(args: argparse.Namespace) -> int:
    key_digits = parse_key_digits(args.key)
    code = args.code.strip()
    if not (code.isdigit() and len(code) == 6):
        print("Code must be exactly 6 digits.", file=sys.stderr)
        return 2

    base_ts = parse_at(args.at)
    window = int(args.window)
    matches: List[str] = []

    for off in range(-window, window + 1):
        ts = base_ts + dt.timedelta(minutes=off)
        gen_code, _ = generate(ts, key_digits, show_steps=False)
        if gen_code == code:
            matches.append(ts.strftime("%Y-%m-%d %H:%M"))

    if matches:
        print("OK")
        if args.show_matches:
            for m in matches:
                print(m)
        return 0

    print("NO")
    return 1


def cmd_sbox(args: argparse.Namespace) -> int:
    key_digits = parse_key_digits(args.key)
    sbox = derive_sbox(key_digits)
    if args.format == "digits":
        print("".join(map(str, sbox)))
    else:
        for i, v in enumerate(sbox):
            print(f"{i} -> {v}")
    return 0


def cmd_manual(_: argparse.Namespace) -> int:
    text = [
        "mTOTP manual steps (v2-min, digits-only key, local time)",
        "",
        "Worked example",
        "   Key (10 digits): 1234598760",
        "   Time (local):    2026-01-17 17:00",
        "",
        "1) Time digits (local time)",
        "   Write the planned login time as YYMMDDHHMM (10 digits).",
        "   Example: 2026-01-17 17:00 -> T = 2601171700",
        "",
        "2) Key digits (shared secret)",
        "   Use exactly 10 digits: k1..k10.",
        "   Example: K = 1234598760",
        "",
        "3) Build S-box from key digits",
        "   perm = unique digits from the key, in first-appearance order,",
        "   then append missing digits 0..9 in increasing order.",
        "   Substitution: S(x) = perm[x].",
        "   Example: perm = 1234598760",
        "     input : 0 1 2 3 4 5 6 7 8 9",
        "     output: 1 2 3 4 5 9 8 7 6 0",
        "",
        "4) Combine time and key (mod 10)",
        "   For i=1..10: c[i] = (t[i] + k[i]) mod 10",
        "   Example: C = 3835669460",
        "",
        "5) Substitute (S-box)",
        "   For i=1..10: c[i] = S(c[i])",
        "   Example: C = 4649880581",
        "",
        "6) Diffuse",
        "   a = c10",
        "   For i=1..10:",
        "     c[i] = (c[i] + a) mod 10",
        "     a = c[i]",
        "   Example: C = 5154200534",
        "",
        "7) Fold to 6 digits",
        "   o1 = (c1 + c6)  mod 10",
        "   o2 = (c2 + c7)  mod 10",
        "   o3 = (c3 + c8)  mod 10",
        "   o4 = (c4 + c9)  mod 10",
        "   o5 = (c5 + c10) mod 10",
        "   o6 = (c1+c2+c3+c4+c5+c6+c7+c8+c9+c10) mod 10",
        "   OTP = o1o2o3o4o5o6",
        "   Example: OTP = 510769",
        "",
        "Timezone note",
        "   Always calculate for the server timezone time",
    ]
    print("\n".join(text))
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="mtotp",
        description="mTOTP reference CLI (manual, human-executable TOTP variant).",
    )
    sub = p.add_subparsers(dest="cmd", required=True)

    gen = sub.add_parser("gen", help="Generate a 6-digit mTOTP code (local time by default).")
    gen.add_argument("--key", required=True, help="Exactly 10 digits (0-9).")
    gen.add_argument("--at", default=None, help="Timestamp: YYYYMMDDHHMM or ISO like 2026-01-17T12:34. Default: now (local).")
    gen.add_argument("--show-steps", action="store_true", help="Print intermediate values for manual comparison.")
    gen.add_argument("--eval", default=None, help="Assumed 6-digit result. Prints true/false and exits with code 0/1.")
    gen.set_defaults(func=cmd_gen)

    ver = sub.add_parser("verify", help="Verify a 6-digit mTOTP code within a minute window.")
    ver.add_argument("--key", required=True, help="Exactly 10 digits (0-9).")
    ver.add_argument("--code", required=True, help="6-digit code to verify.")
    ver.add_argument("--at", default=None, help="Base timestamp. Default: now (local).")
    ver.add_argument("--window", default=1, help="Minute window on each side. Default: 1.")
    ver.add_argument("--show-matches", action="store_true", help="Print matching timestamps if any.")
    ver.set_defaults(func=cmd_verify)

    sbox = sub.add_parser("sbox", help="Print the derived S-box for a key.")
    sbox.add_argument("--key", required=True, help="Exactly 10 digits (0-9).")
    sbox.add_argument("--format", choices=["map", "digits"], default="map", help="Output format.")
    sbox.set_defaults(func=cmd_sbox)

    man = sub.add_parser("manual", help="Print the manual steps for computing mTOTP by hand.")
    man.set_defaults(func=cmd_manual)

    return p


def main(argv: List[str]) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
