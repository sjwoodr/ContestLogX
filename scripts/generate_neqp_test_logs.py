#!/usr/bin/env python3
"""
Generate ~115 QSOs each into the starter NEQP test log files. Deterministic
via a fixed RNG seed for reproducibility. Inserts a small number of
duplicates per log to validate the engine's per-band/mode dupe detection.

Reads:  test_logs/test_neqp_instate.clx     (NE station, K1WES from CTWES)
        test_logs/test_neqp_outofstate.clx  (FL station, N9OH operator)
Writes: same files, with the existing starter QSOs preserved and new
        synthesized QSOs appended.

Usage:  python3 scripts/generate_neqp_test_logs.py [--seed 17]
"""

import argparse
import json
import os
import random
from datetime import datetime, timedelta, timezone

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTEST_FILE = os.path.join(REPO_ROOT, "contests", "neqp.json")
INSTATE_LOG  = os.path.join(REPO_ROOT, "test_logs", "test_neqp_instate.clx")
OOS_LOG      = os.path.join(REPO_ROOT, "test_logs", "test_neqp_outofstate.clx")

# All 6 New England states use call-area 1 (W1, K1, N1, AA1-AL1 etc.).
# So a New England callsign generator just needs to use digit-1.
NE_STATES = ["CT", "ME", "MA", "NH", "RI", "VT"]

# Non-NE US states a station might send. The 6 NE state postal codes are
# excluded — NE ops never receive a 2-letter NE state code (other NE ops
# send 5-letter county codes; non-NE ops send their own state). DC is
# included because it aliases to MD via namedMultAliases.
NON_NE_US_STATES = [
    "AL", "AK", "AZ", "AR", "CA", "CO", "DC", "DE", "FL", "GA",
    "HI", "ID", "IL", "IN", "IA", "KS", "KY", "LA", "MD",
    "MI", "MN", "MS", "MO", "MT", "NE", "NV", "NJ", "NM",
    "NY", "NC", "ND", "OH", "OK", "OR", "PA", "SC", "SD",
    "TN", "TX", "UT", "VA", "WA", "WV", "WI", "WY",
]

# 14 provinces — VO1/VO2 split per NEQP rules. Standard NL replaced by
# VO1 (Newfoundland) + VO2 (Labrador).
PROVINCES = ["NS", "QC", "ON", "MB", "SK", "AB", "BC", "NT", "NB",
             "VO1", "VO2", "NU", "YT", "PE"]

# 5 HF bands — NEQP excludes 160m. Suggested CW/Phone freqs from the rules.
BAND_FREQ_PRESETS = {
    "80m":  {"CW": 3540, "SSB": 3850, "DIGITAL": 3580},
    "40m":  {"CW": 7035, "SSB": 7180, "DIGITAL": 7080},
    "20m":  {"CW": 14040, "SSB": 14280, "DIGITAL": 14080},
    "15m":  {"CW": 21040, "SSB": 21380, "DIGITAL": 21080},
    "10m":  {"CW": 28040, "SSB": 28380, "DIGITAL": 28080},
}

def ssb_mode_for_band(band):
    return "LSB" if band in ("80m", "40m") else "USB"

# Mode → band weighting matched to typical NEQP activity. 20/40m carry
# most of the action.
BAND_WEIGHTS_BY_MODE = {
    "CW":      {"80m": 17, "40m": 33, "20m": 33, "15m": 12, "10m": 5},
    "SSB":     {"80m": 14, "40m": 31, "20m": 35, "15m": 14, "10m": 6},
    "DIGITAL": {"80m": 8,  "40m": 28, "20m": 48, "15m": 12, "10m": 4},
}

# Mode mix target — CW + SSB + DIGITAL, with digital sparse.
MODE_MIX_TARGET = {"CW": 45, "SSB": 50, "DIGITAL": 5}

US_PREFIXES_SINGLE = ["W", "K", "N"]
US_PREFIXES_DOUBLE = ["AA", "AB", "AC", "AD", "AE", "AF", "AG", "AI", "AJ", "AK",
                       "KA", "KB", "KC", "KD", "KE", "KF", "KG", "KI", "KJ", "KK",
                       "KM", "KN", "KO", "NA", "NB", "NC", "ND", "NE", "NF", "NG",
                       "NI", "NJ", "NK", "NN", "NO", "WA", "WB", "WD", "WE", "WF",
                       "WG", "WI", "WJ", "WK", "WM", "WN", "WO"]

CANADIAN_PREFIXES = {
    "NS":  ["VE1", "VA1"],
    "QC":  ["VE2", "VA2"],
    "ON":  ["VE3", "VA3"],
    "MB":  ["VE4", "VA4"],
    "SK":  ["VE5", "VA5"],
    "AB":  ["VE6", "VA6"],
    "BC":  ["VE7", "VA7"],
    "NT":  ["VE8", "VA8"],
    "NB":  ["VE9", "VA9"],
    "VO1": ["VO1"],
    "VO2": ["VO2"],
    "PE":  ["VY2"],
    "YT":  ["VY1"],
    "NU":  ["VY0"],
}

# DX prefixes chosen for unambiguous DXCC mapping: each prefix here maps
# 1:1 to exactly one DXCC entity for any digit 0-9 + suffix. Prefixes like
# EA (Spain main, but EA6=Balearic, EA8=Canary, EA9=Ceuta), VK (VK0/9
# split off Heard, Christmas, Norfolk), ZL (ZL7/8/9 separate), PY (PY0
# separate), CE (CE0 separate), TI (TI9 separate), HK (HK0 separate),
# OH (OH0=Aland), HB9 (would form malformed call HB9{digit}), and ZS
# (ZS8=Marion separate) are excluded because they make manual DXCC
# counting non-trivial. Italy "I" is safe because the only Italy split,
# Sicily, is stored in cty.dat as exact-match "*IT9" and never matches
# plain "I"+digit.
DX_PREFIXES = ["G", "DL", "F", "I", "JA", "PA", "OE", "SM", "OZ", "OK",
               "SP", "LA", "EI", "LU", "XE"]

# US area-digit mapping. NE states all use 1.
STATE_TO_AREA_DIGIT = {
    "CT": "1", "ME": "1", "MA": "1", "NH": "1", "RI": "1", "VT": "1",
    "NY": "2", "NJ": "2",
    "DE": "3", "MD": "3", "PA": "3", "DC": "3",
    "AL": "4", "FL": "4", "GA": "4", "KY": "4", "NC": "4", "SC": "4",
    "TN": "4", "VA": "4",
    "AR": "5", "LA": "5", "MS": "5", "NM": "5", "OK": "5", "TX": "5",
    "CA": "6",
    "AZ": "7", "ID": "7", "MT": "7", "NV": "7", "OR": "7", "UT": "7",
    "WA": "7", "WY": "7",
    "MI": "8", "OH": "8", "WV": "8",
    "IL": "9", "IN": "9", "WI": "9",
    "CO": "0", "IA": "0", "KS": "0", "MN": "0", "MO": "0", "NE": "0",
    "ND": "0", "SD": "0",
    "AK": "L", "HI": "H",
}

def gen_us_call_with_digit(rng, digit):
    if rng.random() < 0.7:
        prefix = rng.choice(US_PREFIXES_SINGLE)
    else:
        prefix = rng.choice(US_PREFIXES_DOUBLE)
    suffix_len = rng.choices([1, 2, 3], weights=[5, 50, 45])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{digit}{suffix}"

def gen_call_for_us_state(rng, state):
    if state == "AK":
        return f"KL7{''.join(rng.choices('ABCDEFGHIJKLMNOPQRSTUVWXYZ', k=rng.choice([2, 3])))}"
    if state == "HI":
        return f"KH6{''.join(rng.choices('ABCDEFGHIJKLMNOPQRSTUVWXYZ', k=rng.choice([2, 3])))}"
    digit = STATE_TO_AREA_DIGIT.get(state, "0")
    return gen_us_call_with_digit(rng, digit)

def gen_ne_call(rng):
    """All 6 New England states use call-area 1."""
    return gen_us_call_with_digit(rng, "1")

def gen_canadian_call(rng, province_code):
    prefix = rng.choice(CANADIAN_PREFIXES[province_code])
    suffix_len = rng.choices([2, 3], weights=[60, 40])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{suffix}"

def gen_dx_call(rng):
    prefix = rng.choice(DX_PREFIXES)
    digit = str(rng.randint(0, 9))
    suffix_len = rng.choices([2, 3], weights=[55, 45])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{digit}{suffix}"

def load_neqp_counties():
    """Group the 68 NE counties by their 2-letter state prefix."""
    with open(CONTEST_FILE, "r") as f:
        contest = json.load(f)
    counties = contest["validation"]["inStateMults"]
    by_state = {s: [] for s in NE_STATES}
    for code in counties:
        prefix = code[:2]
        if prefix in by_state:
            by_state[prefix].append(code)
    return by_state, counties

def pick_mode(rng):
    return rng.choices(list(MODE_MIX_TARGET.keys()),
                       weights=list(MODE_MIX_TARGET.values()))[0]

def pick_band_for_mode(rng, mode):
    weights = BAND_WEIGHTS_BY_MODE[mode]
    bands = list(weights.keys())
    return rng.choices(bands, weights=[weights[b] for b in bands])[0]

def cw_or_ssb_log_mode(mode, band):
    if mode == "CW":
        return "CW"
    if mode == "SSB":
        return ssb_mode_for_band(band)
    if mode == "DIGITAL":
        return "RTTY"
    return mode

def make_qso(qso_id, ts, callsign, band, mode_log, freq, exch_recvd, exch_sent, rst_set, duplicate=False):
    rst_s, rst_r = rst_set
    return {
        "band": band,
        "callsign": callsign,
        "duplicate": duplicate,
        "dxcc_count": 0,
        "exchange_fields": {
            "CALL":  callsign,
            "EXCHr": exch_recvd,
            "EXCHs": exch_sent,
            "NAMEs": "Steve",
            "RSTr":  rst_r,
            "RSTs":  rst_s,
            "SNs":   str(qso_id),
        },
        "frequency": freq,
        "grid_square_count": 0,
        "id": qso_id,
        "mode": mode_log,
        "multiplier_count": 0,
        "points":           0,
        "rst_received": rst_r,
        "rst_sent":     rst_s,
        "serial": qso_id,
        "timestamp": ts.strftime("%Y-%m-%dT%H:%M:%SZ"),
    }

def rst_for_mode(mode):
    return ("599", "599") if mode in ("CW", "DIGITAL") else ("59", "59")

def insert_dupes(qsos, rng, count, next_id_start):
    out = []
    next_id = next_id_start
    candidates = [q for q in qsos if not q.get("duplicate", False)]
    for _ in range(count):
        original = rng.choice(candidates)
        orig_ts = datetime.fromisoformat(original["timestamp"].replace("Z", "+00:00"))
        dupe_ts = orig_ts + timedelta(seconds=rng.randint(60, 240))
        dupe = make_qso(
            qso_id=next_id, ts=dupe_ts,
            callsign=original["callsign"],
            band=original["band"],
            mode_log=original["mode"],
            freq=original["frequency"],
            exch_recvd=original["exchange_fields"]["EXCHr"],
            exch_sent=original["exchange_fields"]["EXCHs"],
            rst_set=(original["rst_sent"], original["rst_received"]),
            duplicate=True,
        )
        out.append(dupe)
        next_id += 1
    return out

def gen_qsos_for_instate(starter, rng, county_by_state, all_counties, target_total=115, dupe_count=3):
    """K1WES from CTWES (CT Western Council). Works EVERYONE.

    Mix:
      ~30% other NE stations (5-letter county codes from any of 6 states,
            except own CTWES — they DO credit a county multiplier per the
            rules, unlike DEQP's DE-DE no-mult rule),
      ~30% non-NE US (2-letter state codes — the bulk of state mults),
      ~15% Canadian (2-letter province codes incl. VO1/VO2),
      ~25% DX (DXCC entity credit since NE ops include the dxcc category).
    """
    out = []
    next_id = starter["qsos"][-1]["id"] + 1
    last_ts = datetime.fromisoformat(starter["qsos"][-1]["timestamp"].replace("Z", "+00:00"))
    used_keys = set()
    for q in starter["qsos"]:
        used_keys.add((q["callsign"], q["band"],
                       "SSB" if q["mode"] in ("USB", "LSB") else
                       ("DIGITAL" if q["mode"] in ("RTTY", "PSK") else q["mode"])))

    needed_real = target_total - len(starter["qsos"]) - dupe_count
    operator_exch_sent = "CTWES"
    own_county = "CTWES"

    for _ in range(needed_real):
        # 20-hour contest, ~115 QSOs → average ~10 minutes apart.
        last_ts = last_ts + timedelta(seconds=rng.randint(180, 700))
        mode = pick_mode(rng)
        band = pick_band_for_mode(rng, mode)
        log_mode = cw_or_ssb_log_mode(mode, band)
        freq = BAND_FREQ_PRESETS[band][mode]

        cat_roll = rng.random()
        if cat_roll < 0.30:
            # Other NE station — pick any NE state, then any county. Skip
            # own county for variety on multipliers.
            their_county = rng.choice(all_counties)
            while their_county == own_county:
                their_county = rng.choice(all_counties)
            exch_r = their_county
            call = gen_ne_call(rng)
        elif cat_roll < 0.60:
            their_state = rng.choice(NON_NE_US_STATES)
            exch_r = their_state
            call = gen_call_for_us_state(rng, their_state)
        elif cat_roll < 0.75:
            province = rng.choice(PROVINCES)
            exch_r = province
            call = gen_canadian_call(rng, province)
        else:
            exch_r = "DX"
            call = gen_dx_call(rng)

        for _ in range(20):
            if (call, band, mode) not in used_keys:
                break
            if exch_r == "DX":
                call = gen_dx_call(rng)
            elif len(exch_r) == 5:
                call = gen_ne_call(rng)
            elif exch_r in PROVINCES:
                call = gen_canadian_call(rng, exch_r)
            else:
                call = gen_call_for_us_state(rng, exch_r)
        used_keys.add((call, band, mode))

        rst_set = rst_for_mode(mode)
        out.append(make_qso(next_id, last_ts, call, band, log_mode, freq,
                            exch_r, operator_exch_sent, rst_set))
        next_id += 1

    out += insert_dupes(out + starter["qsos"], rng, dupe_count, next_id)
    return out

def gen_qsos_for_outofstate(starter, rng, county_by_state, all_counties, target_total=115, dupe_count=2):
    """N9OH (FL) works only NE. Every received exchange is one of the 68
    NE county codes; calls are W1/K1/N1 area-1 calls."""
    out = []
    next_id = starter["qsos"][-1]["id"] + 1
    last_ts = datetime.fromisoformat(starter["qsos"][-1]["timestamp"].replace("Z", "+00:00"))
    used_keys = set()
    for q in starter["qsos"]:
        used_keys.add((q["callsign"], q["band"],
                       "SSB" if q["mode"] in ("USB", "LSB") else
                       ("DIGITAL" if q["mode"] in ("RTTY", "PSK") else q["mode"])))

    needed_real = target_total - len(starter["qsos"]) - dupe_count
    operator_exch_sent = "FL"

    for _ in range(needed_real):
        last_ts = last_ts + timedelta(seconds=rng.randint(180, 700))
        mode = pick_mode(rng)
        band = pick_band_for_mode(rng, mode)
        log_mode = cw_or_ssb_log_mode(mode, band)
        freq = BAND_FREQ_PRESETS[band][mode]

        exch_r = rng.choice(all_counties)
        call = gen_ne_call(rng)

        for _ in range(20):
            if (call, band, mode) not in used_keys:
                break
            call = gen_ne_call(rng)
        used_keys.add((call, band, mode))

        rst_set = rst_for_mode(mode)
        out.append(make_qso(next_id, last_ts, call, band, log_mode, freq,
                            exch_r, operator_exch_sent, rst_set))
        next_id += 1

    out += insert_dupes(out + starter["qsos"], rng, dupe_count, next_id)
    return out

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=17,
                        help="RNG seed for reproducible output (default 17)")
    parser.add_argument("--total", type=int, default=115,
                        help="Target total QSOs per file (default 115)")
    args = parser.parse_args()

    county_by_state, all_counties = load_neqp_counties()

    for path, gen_fn, label, _ in [
        (INSTATE_LOG, gen_qsos_for_instate,    "in-state (NE)",     None),
        (OOS_LOG,     gen_qsos_for_outofstate, "out-of-state (FL)", None),
    ]:
        with open(path, "r") as f:
            log = json.load(f)
        # Random dupe count via the seeded RNG (1-4) — same seed gives same
        # count, and the same RNG instance gives a fresh value for each log.
        rng = random.Random(args.seed)
        actual_dupes = rng.choice([1, 2, 3, 4])
        # Restart the RNG so the QSO sequence is stable regardless of
        # whether we burned a draw on the dupe count.
        rng = random.Random(args.seed)
        new_qsos = gen_fn(log, rng, county_by_state, all_counties,
                          target_total=args.total, dupe_count=actual_dupes)
        log["qsos"].extend(new_qsos)
        log["statistics"] = {
            "score": 0,
            "total_points": 0,
            "total_qsos": len(log["qsos"]),
        }
        log["modified"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S")
        with open(path, "w") as f:
            json.dump(log, f, indent=4)
            f.write("\n")
        # Unique DXCC entities used for DX QSOs — by construction (the
        # curated DX_PREFIXES list maps 1:1 to DXCC entities), this is
        # exactly what the engine should count for `dxccMultipliers`.
        dx_prefixes_used = set()
        for q in log["qsos"]:
            if q.get("duplicate", False):
                continue
            if q["exchange_fields"]["EXCHr"] == "DX":
                call = q["callsign"]
                # Match against safe-list prefixes (longest first).
                for p in sorted(DX_PREFIXES, key=len, reverse=True):
                    if call.startswith(p):
                        dx_prefixes_used.add(p)
                        break
        print(f"{label}: now {len(log['qsos'])} QSOs ({len(new_qsos)} added, "
              f"{actual_dupes} flagged as duplicates) -> {path}")
        if dx_prefixes_used:
            print(f"  unique DXCC entities (DX QSOs): {len(dx_prefixes_used)} "
                  f"-> {sorted(dx_prefixes_used)}")

if __name__ == "__main__":
    main()
