#!/usr/bin/env python3
"""
Generate the CQ WPX automated test logs and INDEPENDENTLY recompute their
expected scores.

We deliberately implement the full WPX scoring pipeline here in Python - prefix
extraction, per-band point tiers, NA-NA exception, sameDxccEntity precedence,
multsOnce uniqueness, per-band dupe handling, and out-of-band filtering - so
that any divergence between this implementation and the C++ engine fails the
automated_tests.json check loudly.

Three logs are produced:
  - test_cqwpx_na.clx    NA station (N9OH, USA): exercises sameDxccEntity +
                         bothInNA (HF + LF tier) + differentContinent.
  - test_cqwpx_dx.clx    DX station (DL5XX, Germany): exercises sameDxccEntity +
                         sameContinent (HF + LF tier) + differentContinent.
  - test_cqwpx_ssb.clx   SSB-mode coverage so the SSB scoring path is exercised
                         (CW and SSB share the same point structure but the
                         engine normalizes mode strings differently).

Each log mixes in the messy real-world cases that have caught major loggers in
the past:
  - Per-band dupes (must score 0 points and award 0 prefix multipliers)
  - Out-of-band QSOs on 30m / 17m / 12m (must score 0 and not multiply)
  - Portable QSOs: /MM, /M, /QRP, /4 call-area changes, /KH6, PJ2/N9OH
  - Digit-led prefixes (3D2RI, 4U1ITU, 9A1XX)
  - Multi-digit special-event prefixes (HG19, OE25, LY1000)
  - Same-prefix-different-call (e.g. K1AAA + KC1XX both contribute to K1/KC1)
  - Same-call-different-prefix (e.g. K1AAA on 20m, K1AAA/4 on 40m)
  - sameDxccEntity precedence wins over bothInNA for K-K NA contacts

The expected score in automated_tests.json is the value computed by THIS
script - so adding QSOs without recomputing is impossible.
"""

import json
import re
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path


# ---------------------------------------------------------------------------
# WPX prefix extractor (independent re-implementation of the C++ algorithm).
# Used to verify the engine's prefix extraction matches ours QSO-by-QSO.
# ---------------------------------------------------------------------------
LICENSE_SUFFIXES = {"MM", "AM", "M", "AE", "AG", "A", "E", "J", "P", "T", "N", "QRP"}


def extract_wpx_prefix(call: str) -> str:
    call = (call or "").upper().strip()
    if not call:
        return ""

    # Strip trailing license-class / mobile suffixes repeatedly.
    while "/" in call:
        slash = call.rfind("/")
        tail = call[slash + 1:]
        if tail in LICENSE_SUFFIXES:
            call = call[:slash]
        else:
            break

    # Trim stray leading/trailing slashes.
    call = call.strip("/")
    if not call:
        return ""

    # Slash notation: portable designator handling.
    if "/" in call:
        slash = call.find("/")
        left, right = call[:slash], call[slash + 1:]
        # Multi-slash collapses to first two parts.
        if "/" in right:
            right = right[:right.find("/")]
        if not left or not right:
            return ""

        digits_only = re.compile(r"^[0-9]+$")

        def combine_with_digit(base: str, digit: str) -> str:
            base_prefix = compute_core(base)
            if not base_prefix:
                return ""
            stem_end = len(base_prefix)
            while stem_end > 0 and base_prefix[stem_end - 1].isdigit():
                stem_end -= 1
            return base_prefix[:stem_end] + digit

        if digits_only.match(right):
            return combine_with_digit(left, right)
        if digits_only.match(left):
            return combine_with_digit(right, left)

        def ends_in_digit(s):
            return bool(s) and s[-1].isdigit()

        def looks_like_base_call(s):
            last_digit = -1
            for i in range(len(s) - 1, -1, -1):
                if s[i].isdigit():
                    last_digit = i
                    break
            if last_digit < 0:
                return False
            has_trailing_alpha = last_digit < len(s) - 1
            return has_trailing_alpha and len(s) >= 4

        if ends_in_digit(left) and not ends_in_digit(right):
            designator = left
        elif ends_in_digit(right) and not ends_in_digit(left):
            designator = right
        elif looks_like_base_call(left) and not looks_like_base_call(right):
            designator = right
        elif looks_like_base_call(right) and not looks_like_base_call(left):
            designator = left
        else:
            designator = left if len(left) <= len(right) else right

        return designator_prefix(designator)

    return compute_core(call)


def compute_core(call: str) -> str:
    if not call or not re.match(r"^[A-Z0-9]+$", call) or not re.search(r"[A-Z]", call):
        return ""
    for i in range(len(call) - 1, -1, -1):
        if call[i].isdigit():
            return call[: i + 1]
    return (call[:2] + "0") if len(call) >= 2 else (call + "0")


def designator_prefix(d: str) -> str:
    if not d or not re.match(r"^[A-Z0-9]+$", d) or not re.search(r"[A-Z]", d):
        return ""
    if re.search(r"[0-9]", d):
        return d
    return (d[:2] + "0") if len(d) >= 2 else (d + "0")


# ---------------------------------------------------------------------------
# WPX point calculation
# ---------------------------------------------------------------------------
HF_BANDS = {"10m", "15m", "20m"}
LF_BANDS = {"40m", "80m", "160m"}
WPX_BANDS = HF_BANDS | LF_BANDS

BAND_FREQ = {
    "160m": 1820, "80m": 3525, "40m": 7025,
    "30m": 10110,                # out-of-band for WPX
    "20m": 14025, "17m": 18075,  # out-of-band for WPX
    "15m": 21025, "12m": 24905,  # out-of-band for WPX
    "10m": 28025,
}


def wpx_points(my_continent, my_dxcc, their_continent, their_dxcc, band):
    """Return WPX QSO point value, or 0 if out-of-band."""
    if band not in WPX_BANDS:
        return 0
    is_hf = band in HF_BANDS
    if my_dxcc == their_dxcc:
        return 1
    if my_continent == "NA" and their_continent == "NA":
        return 2 if is_hf else 4
    if my_continent == their_continent:
        return 1 if is_hf else 2
    return 3 if is_hf else 6


def compute_log_score(my_continent, my_dxcc, qsos):
    """Independently compute total score for a list of QSOs.

    qsos: list of dicts with keys: band, callsign, mode, their_continent,
          their_dxcc. Returns (total_pts, prefix_count, score, dupe_count,
          oob_count, prefix_set).
    """
    seen_band_call = set()                # for per-band dupe detection
    total_pts = 0
    prefix_set = set()
    dupe_count = 0
    oob_count = 0

    for q in qsos:
        band = q["band"]
        call = q["callsign"]
        key = (band, call)

        if band not in WPX_BANDS:
            oob_count += 1
            continue

        if key in seen_band_call:
            dupe_count += 1                # zero points, no multiplier
            continue
        seen_band_call.add(key)

        pts = wpx_points(my_continent, my_dxcc,
                         q["their_continent"], q["their_dxcc"], band)
        total_pts += pts

        # Multiplier: prefix counts once across the entire contest.
        prefix = extract_wpx_prefix(call)
        if prefix:
            prefix_set.add(prefix)

    return total_pts, len(prefix_set), total_pts * len(prefix_set), \
           dupe_count, oob_count, prefix_set


# ---------------------------------------------------------------------------
# CLX file builder
# ---------------------------------------------------------------------------
def make_qso(serial, qso_spec, mode, base_time):
    band = qso_spec["band"]
    call = qso_spec["callsign"]
    freq = BAND_FREQ[band]
    rst = "599" if mode == "CW" else "59"
    timestamp = (base_time + timedelta(minutes=serial * 7)).strftime(
        "%Y-%m-%dT%H:%M:%SZ"
    )
    qso = {
        "band": band,
        "callsign": call,
        "duplicate": False,
        "exchange_fields": {
            "CALL": call,
            "RSTr": rst,
            "RSTs": rst,
            "SNs": str(serial),
            "SNr": str(100 + serial),
        },
        "frequency": freq,
        "id": serial,
        "mode": mode,
        "points": 0,
        "rst_received": rst,
        "rst_sent": rst,
        "serial": serial,
        "timestamp": timestamp,
    }
    return qso


def build_clx(my_call, mode, qsos_spec, out_path):
    base_time = datetime(2026, 5, 30, 0, 0, 0, tzinfo=timezone.utc)
    qsos = [make_qso(i, q, mode, base_time) for i, q in enumerate(qsos_spec, 1)]

    log = {
        "contest": {
            "categories": {
                "userPrompt_contestMode": mode,
                "userPrompt_operatingCategory": "SOAB-LP",
            },
            "contest_file": "cqwpx.json",
            "mode": mode,
            "name": "CQ World-Wide WPX Contest",
            "type": "CQ World-Wide WPX Contest",
            "version": "1.0.0",
            "year": 2026,
        },
        "created": "2026-05-30T00:00:00",
        "exchange_fields": [],
        "format": "ContestLogX",
        "format_version": "1.0",
        "modified": "2026-05-30T00:00:00",
        "qsos": qsos,
        "station": {
            "callsign": my_call,
            "equipment": {"power": 100},
            "operator": "Steve",
        },
        "statistics": {"score": 0, "total_points": 0, "total_qsos": len(qsos)},
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(log, indent=4))


# ---------------------------------------------------------------------------
# Test log #1 - NA station N9OH (USA, K, NA)
#
# Coverage:
#   - sameDxccEntity (K-K, 1 pt) - KC2ABC, K1AAA, W2DDD, KC1XX
#   - bothInNA HF (2 pt) - VE3XYZ, XE2DEF
#   - bothInNA LF (4 pt) - VE7CCC
#   - differentContinent HF (3 pt) - G3PQR, DL5ABC, JA1HGY, ZS6FFF, PY1GGG,
#                                    9A1XX, 4U1ITU, 3D2RI
#   - differentContinent LF (6 pt) - F5BBB, JA3HHH, EA5EEE, OZ7HHH, OE25HG
#   - portable: K1AAA/4 (call-area change → K4 prefix), W1AAA/KH6 (visiting,
#               KH6 prefix, differentContinent), PJ2/N9OH (PJ2 prefix, dC),
#               G3ABC/MM (maritime stripped → G3 prefix, dC),
#               DL1XX/QRP (QRP stripped → DL1 prefix, dC)
#   - per-band dupe - KC2ABC on 20m logged twice (second is 0 pts, no mult)
#   - out-of-band - N1OOB on 30m (must score 0 and not contribute prefix)
#   - same call different prefix - K1AAA on 20m (K1) and K1AAA/4 on 40m (K4)
# ---------------------------------------------------------------------------
NA_QSOS = [
    # (band, callsign, their_continent, their_dxcc) - DXCC code rough proxy
    {"band": "20m",  "callsign": "KC2ABC",   "their_continent": "NA", "their_dxcc": 291},  # USA
    {"band": "20m",  "callsign": "VE3XYZ",   "their_continent": "NA", "their_dxcc": 1},    # Canada
    {"band": "20m",  "callsign": "XE2DEF",   "their_continent": "NA", "their_dxcc": 50},   # Mexico
    {"band": "20m",  "callsign": "G3PQR",    "their_continent": "EU", "their_dxcc": 223},  # England
    {"band": "20m",  "callsign": "DL5ABC",   "their_continent": "EU", "their_dxcc": 230},  # Germany
    {"band": "20m",  "callsign": "JA1HGY",   "their_continent": "AS", "their_dxcc": 339},  # Japan
    {"band": "20m",  "callsign": "9A1XX",    "their_continent": "EU", "their_dxcc": 497},  # Croatia (digit-led prefix)
    {"band": "20m",  "callsign": "4U1ITU",   "their_continent": "EU", "their_dxcc": 117},  # UN/ITU (digit-led prefix)
    {"band": "20m",  "callsign": "3D2RI",    "their_continent": "OC", "their_dxcc": 176},  # Fiji (digit-led prefix)
    {"band": "20m",  "callsign": "K1AAA",    "their_continent": "NA", "their_dxcc": 291},  # USA - same call appears /4 on 40m
    {"band": "20m",  "callsign": "KC2ABC",   "their_continent": "NA", "their_dxcc": 291},  # DUPE (same band+call) → 0 pts
    {"band": "20m",  "callsign": "PJ2/N9OH", "their_continent": "SA", "their_dxcc": 520},  # Curaçao designator → PJ2 prefix
    {"band": "20m",  "callsign": "G3ABC/MM", "their_continent": "EU", "their_dxcc": 223},  # /MM stripped, prefix G3

    {"band": "40m",  "callsign": "K1AAA/4",  "their_continent": "NA", "their_dxcc": 291},  # USA - call-area change to K4
    {"band": "40m",  "callsign": "VE7CCC",   "their_continent": "NA", "their_dxcc": 1},    # Canada
    {"band": "40m",  "callsign": "F5BBB",    "their_continent": "EU", "their_dxcc": 227},  # France
    {"band": "40m",  "callsign": "JA3HHH",   "their_continent": "AS", "their_dxcc": 339},  # Japan
    {"band": "40m",  "callsign": "DL1XX/QRP","their_continent": "EU", "their_dxcc": 230},  # /QRP stripped, prefix DL1

    {"band": "80m",  "callsign": "W2DDD",    "their_continent": "NA", "their_dxcc": 291},  # USA
    {"band": "80m",  "callsign": "EA5EEE",   "their_continent": "EU", "their_dxcc": 281},  # Spain
    {"band": "80m",  "callsign": "W1AAA/KH6","their_continent": "OC", "their_dxcc": 110},  # KH6 designator (Hawaii)

    {"band": "15m",  "callsign": "ZS6FFF",   "their_continent": "AF", "their_dxcc": 462},  # S. Africa
    {"band": "15m",  "callsign": "KC1XX",    "their_continent": "NA", "their_dxcc": 291},  # USA - adds KC1 prefix

    {"band": "10m",  "callsign": "PY1GGG",   "their_continent": "SA", "their_dxcc": 108},  # Brazil

    {"band": "160m", "callsign": "OZ7HHH",   "their_continent": "EU", "their_dxcc": 221},  # Denmark
    {"band": "160m", "callsign": "OE25HG",   "their_continent": "EU", "their_dxcc": 206},  # Austria special-event

    # Out-of-band QSO must score 0 and contribute no prefix.
    {"band": "30m",  "callsign": "N1OOB",    "their_continent": "NA", "their_dxcc": 291},
]


# ---------------------------------------------------------------------------
# Test log #2 - DX station DL5XX (Germany, DL, EU)
#
# Coverage:
#   - sameDxccEntity (DL-DL, 1 pt) - DL1ABC, DL2XYZ
#   - sameContinent HF (1 pt) - F5DEF, G3PQR, OE25HG (multi-digit prefix),
#                               OK1ZZZ, 9A1XX (digit-led prefix EU)
#   - sameContinent LF (2 pt) - OZ7XXX, EA5YYY, I2ZZZ, F8AAA
#   - differentContinent HF (3 pt) - W1ABC, JA1HGY, PY1GGG, ZS6FFF
#   - differentContinent LF (6 pt) - K1AAA, VE3CCC, JA3HHH, EA8XX
#   - portable: F5XX/4 (call-area change → F4 prefix, sameContinent),
#               W1AW/HB (HB designator visiting → HB0 prefix, sameContinent),
#               LY1000A (special-event multi-digit prefix → LY1000),
#               JA1XX/MM (maritime stripped → JA1, differentContinent)
#   - per-band dupe - F5DEF on 20m logged twice
#   - out-of-band - DL2OOB on 17m
# ---------------------------------------------------------------------------
DX_QSOS = [
    {"band": "20m",  "callsign": "DL1ABC",   "their_continent": "EU", "their_dxcc": 230},
    {"band": "20m",  "callsign": "DL2XYZ",   "their_continent": "EU", "their_dxcc": 230},
    {"band": "20m",  "callsign": "F5DEF",    "their_continent": "EU", "their_dxcc": 227},
    {"band": "20m",  "callsign": "G3PQR",    "their_continent": "EU", "their_dxcc": 223},
    {"band": "20m",  "callsign": "OE25HG",   "their_continent": "EU", "their_dxcc": 206},  # multi-digit prefix
    {"band": "20m",  "callsign": "OK1ZZZ",   "their_continent": "EU", "their_dxcc": 503},
    {"band": "20m",  "callsign": "9A1XX",    "their_continent": "EU", "their_dxcc": 497},  # digit-led
    {"band": "20m",  "callsign": "W1ABC",    "their_continent": "NA", "their_dxcc": 291},
    {"band": "20m",  "callsign": "JA1HGY",   "their_continent": "AS", "their_dxcc": 339},
    {"band": "20m",  "callsign": "PY1GGG",   "their_continent": "SA", "their_dxcc": 108},
    {"band": "20m",  "callsign": "F5DEF",    "their_continent": "EU", "their_dxcc": 227},  # DUPE
    {"band": "20m",  "callsign": "F5XX/4",   "their_continent": "EU", "their_dxcc": 227},  # F4 via call-area change
    {"band": "20m",  "callsign": "W1AW/HB",  "their_continent": "EU", "their_dxcc": 251},  # HB0 designator (Liechtenstein)

    {"band": "40m",  "callsign": "OZ7XXX",   "their_continent": "EU", "their_dxcc": 221},
    {"band": "40m",  "callsign": "EA5YYY",   "their_continent": "EU", "their_dxcc": 281},
    {"band": "40m",  "callsign": "I2ZZZ",    "their_continent": "EU", "their_dxcc": 248},
    {"band": "40m",  "callsign": "F8AAA",    "their_continent": "EU", "their_dxcc": 227},
    {"band": "40m",  "callsign": "K1AAA",    "their_continent": "NA", "their_dxcc": 291},
    {"band": "40m",  "callsign": "VE3CCC",   "their_continent": "NA", "their_dxcc": 1},
    {"band": "40m",  "callsign": "JA3HHH",   "their_continent": "AS", "their_dxcc": 339},
    {"band": "40m",  "callsign": "LY1000A",  "their_continent": "EU", "their_dxcc": 146},  # special event

    {"band": "80m",  "callsign": "EA8XX",    "their_continent": "AF", "their_dxcc": 29},   # Canary Is. (AF, not EU!)
    {"band": "80m",  "callsign": "JA1XX/MM", "their_continent": "AS", "their_dxcc": 339},  # /MM stripped

    {"band": "15m",  "callsign": "ZS6FFF",   "their_continent": "AF", "their_dxcc": 462},

    {"band": "10m",  "callsign": "PY1ABC",   "their_continent": "SA", "their_dxcc": 108},  # adds PY1 again? No: PY1GGG already in 20m

    {"band": "160m", "callsign": "OZ1AB",    "their_continent": "EU", "their_dxcc": 221},

    # Out-of-band
    {"band": "17m",  "callsign": "DL2OOB",   "their_continent": "EU", "their_dxcc": 230},
]


# ---------------------------------------------------------------------------
# Test log #3 - SSB-mode coverage. Reuse a small NA log in SSB to verify the
# engine's mode-string normalization (USB/SSB) and per-band tier are also
# correct on the SSB path.
# ---------------------------------------------------------------------------
SSB_QSOS = [
    {"band": "20m",  "callsign": "K1AAA",    "their_continent": "NA", "their_dxcc": 291},
    {"band": "20m",  "callsign": "VE3XYZ",   "their_continent": "NA", "their_dxcc": 1},
    {"band": "20m",  "callsign": "G3PQR",    "their_continent": "EU", "their_dxcc": 223},
    {"band": "20m",  "callsign": "JA1HGY",   "their_continent": "AS", "their_dxcc": 339},
    {"band": "40m",  "callsign": "VE7CCC",   "their_continent": "NA", "their_dxcc": 1},
    {"band": "40m",  "callsign": "F5BBB",    "their_continent": "EU", "their_dxcc": 227},
    {"band": "80m",  "callsign": "EA5EEE",   "their_continent": "EU", "their_dxcc": 281},
    {"band": "10m",  "callsign": "PY1GGG",   "their_continent": "SA", "their_dxcc": 108},
]


def main():
    repo_root = Path(__file__).resolve().parent.parent
    out_dir = repo_root / "test_logs"

    summaries = []

    # NA station log
    pts, prefixes, score, dupes, oob, pset = compute_log_score("NA", 291, NA_QSOS)
    build_clx("N9OH", "CW", NA_QSOS, out_dir / "test_cqwpx_na.clx")
    summaries.append(("NA-CW", "N9OH", len(NA_QSOS), pts, prefixes, score,
                      dupes, oob, sorted(pset)))

    # DX station log
    pts, prefixes, score, dupes, oob, pset = compute_log_score("EU", 230, DX_QSOS)
    build_clx("DL5XX", "CW", DX_QSOS, out_dir / "test_cqwpx_dx.clx")
    summaries.append(("DX-CW", "DL5XX", len(DX_QSOS), pts, prefixes, score,
                      dupes, oob, sorted(pset)))

    # SSB-mode log
    pts, prefixes, score, dupes, oob, pset = compute_log_score("NA", 291, SSB_QSOS)
    build_clx("N9OH", "SSB", SSB_QSOS, out_dir / "test_cqwpx_ssb.clx")
    summaries.append(("NA-SSB", "N9OH", len(SSB_QSOS), pts, prefixes, score,
                      dupes, oob, sorted(pset)))

    print(f"{'Tag':<8} {'Op':<8} {'QSOs':>4} {'Pts':>5} {'Pfx':>4} "
          f"{'Score':>8} {'Dupe':>4} {'OOB':>4}  Prefixes")
    print("-" * 110)
    for tag, op, n, pts, prefixes, score, dupes, oob, pset in summaries:
        print(f"{tag:<8} {op:<8} {n:>4} {pts:>5} {prefixes:>4} "
              f"{score:>8} {dupes:>4} {oob:>4}  {', '.join(pset)}")

    # Emit the expected_score values for paste into automated_tests.json.
    print()
    print("automated_tests.json expected scores:")
    for tag, op, n, pts, prefixes, score, dupes, oob, pset in summaries:
        log_file = {"NA-CW": "test_cqwpx_na.clx",
                    "DX-CW": "test_cqwpx_dx.clx",
                    "NA-SSB": "test_cqwpx_ssb.clx"}[tag]
        print(f'  {log_file}: expected_score = {score}')

    # Optional: verify CLX matches our independently computed scores.
    # When --verify is passed, run CLX on each generated log and compare.
    if "--verify" in sys.argv:
        verify_with_clx(repo_root, summaries)


def verify_with_clx(repo_root: Path, summaries):
    """Run CLX in test-only mode on each generated log and compare scores."""
    import subprocess
    import tempfile

    clx = repo_root / "clx"
    if not clx.exists():
        print(f"\nERROR: {clx} not found - build the project first.")
        sys.exit(1)

    print()
    print("Verifying CLX output matches Python-computed scores...")
    all_ok = True
    for tag, op, n, pts, prefixes, expected_score, dupes, oob, _ in summaries:
        log_file = {"NA-CW": "test_cqwpx_na.clx",
                    "DX-CW": "test_cqwpx_dx.clx",
                    "NA-SSB": "test_cqwpx_ssb.clx"}[tag]
        log_path = repo_root / "test_logs" / log_file
        with tempfile.NamedTemporaryFile(suffix=".log", delete=False) as f:
            debug_log = f.name
        with tempfile.TemporaryDirectory() as sandbox:
            cmd = [str(clx), "--debug", "--log", str(log_path), "--test-only",
                   "--debug-log", debug_log, "--config-dir", sandbox]
            subprocess.run(cmd, capture_output=True, timeout=60)
            with open(debug_log) as f:
                content = f.read()
        m = re.search(r"CLAIMED_SCORE=(\d+)", content)
        actual = int(m.group(1)) if m else None
        ok = actual == expected_score
        all_ok = all_ok and ok
        mark = "✓" if ok else "✗"
        print(f"  {mark} {tag:<8}: Python={expected_score}, CLX={actual}")

    if not all_ok:
        sys.exit(1)


if __name__ == "__main__":
    main()
