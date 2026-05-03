#!/usr/bin/env python3
"""
Generate ~110 QSOs each into the starter INQP test log files. Deterministic
via a fixed RNG seed so the produced logs are stable across re-runs and
across contributors verifying scoring changes. Intentionally inserts a
small number of duplicates to validate the engine's per-band/mode dupe
detection.

Reads:  test_logs/test_inqp_instate.clx     (IN station, KX9IO from INBEN)
        test_logs/test_inqp_outofstate.clx  (FL station, N9OH operator)
Writes: same files, with the existing starter QSOs preserved and new
        synthesized QSOs appended.

Usage:  python3 scripts/generate_inqp_test_logs.py [--seed 11]
"""

import argparse
import json
import os
import random
from datetime import datetime, timedelta, timezone

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTEST_FILE = os.path.join(REPO_ROOT, "contests", "inqp.json")
INSTATE_LOG  = os.path.join(REPO_ROOT, "test_logs", "test_inqp_instate.clx")
OOS_LOG      = os.path.join(REPO_ROOT, "test_logs", "test_inqp_outofstate.clx")

# US states a non-IN station might send. Includes IN itself (no skipping
# needed — IN ops don't see "IN" 2-letter codes from anyone, and non-IN
# ops don't work non-IN stations). Excluding IN from the in-state op's
# multiplier list happens at the contest-definition level via "the other
# 49 US states" — we just don't generate "IN" as a received exchange.
NON_IN_US_STATES = [
    "AL", "AK", "AZ", "AR", "CA", "CO", "CT", "DE", "DC", "FL",
    "GA", "HI", "ID", "IL", "IA", "KS", "KY", "LA", "ME", "MD",
    "MA", "MI", "MN", "MS", "MO", "MT", "NE", "NV", "NH", "NJ",
    "NM", "NY", "NC", "ND", "OH", "OK", "OR", "PA", "RI", "SC",
    "SD", "TN", "TX", "UT", "VT", "VA", "WA", "WV", "WI", "WY",
]

PROVINCES = ["NS", "QC", "ON", "MB", "SK", "AB", "BC", "NT", "NB", "NL", "NU", "YT", "PE"]

# Band frequency layout — must align with contests/inqp.json frequency
# ranges so the engine accepts each QSO. INQP is CW + SSB only — no
# digital.
BAND_FREQ_PRESETS = {
    "160m": {"CW": 1820, "SSB": 1845},   # INQP suggested SSB freq 1.845
    "80m":  {"CW": 3530, "SSB": 3820},   # Suggested SSB 3.820
    "40m":  {"CW": 7035, "SSB": 7190},   # Suggested SSB 7.190
    "20m":  {"CW": 14040, "SSB": 14250}, # Suggested SSB 14.250
    "15m":  {"CW": 21035, "SSB": 21300}, # Suggested SSB 21.300
    "10m":  {"CW": 28035, "SSB": 28400}, # Suggested SSB 28.400
}

def ssb_mode_for_band(band):
    """LSB on 80/40/160, USB above — standard ham radio convention."""
    return "LSB" if band in ("160m", "80m", "40m") else "USB"

# Mode → band weighting matched to typical INQP activity. 20m and 40m carry
# most of the action; 160m is sparse and almost CW-only.
BAND_WEIGHTS_BY_MODE = {
    "CW":  {"160m": 4,  "80m": 14, "40m": 30, "20m": 33, "15m": 14, "10m": 5},
    "SSB": {"160m": 2,  "80m": 11, "40m": 28, "20m": 38, "15m": 16, "10m": 5},
}

# Modes for INQP — CW and SSB only. ~50/50 split.
MODE_MIX_TARGET = {"CW": 50, "SSB": 50}

# US callsign prefix pool. Keeping it inline rather than importing
# scripts/generate_callsigns.py because we need digit control (W9 for
# IN-area calls, W4 for FL etc.).
US_PREFIXES_SINGLE = ["W", "K", "N"]
US_PREFIXES_DOUBLE = ["AA", "AB", "AC", "AD", "AE", "AF", "AG", "AI", "AJ", "AK",
                       "KA", "KB", "KC", "KD", "KE", "KF", "KG", "KI", "KJ", "KK",
                       "KM", "KN", "KO", "NA", "NB", "NC", "ND", "NE", "NF", "NG",
                       "NI", "NJ", "NK", "NN", "NO", "WA", "WB", "WD", "WE", "WF",
                       "WG", "WI", "WJ", "WK", "WM", "WN", "WO"]

CANADIAN_PREFIXES = {
    "NS": ["VE1", "VA1"],
    "QC": ["VE2", "VA2"],
    "ON": ["VE3", "VA3"],
    "MB": ["VE4", "VA4"],
    "SK": ["VE5", "VA5"],
    "AB": ["VE6", "VA6"],
    "BC": ["VE7", "VA7"],
    "NT": ["VE8", "VA8"],
    "NB": ["VE9", "VA9"],
    "NL": ["VO1", "VO2"],
    "PE": ["VY2"],
    "YT": ["VY1"],
    "NU": ["VY0"],
}

DX_PREFIXES = ["G", "DL", "F", "I", "JA", "EA", "PA", "OE", "OH", "SM",
               "OZ", "OK", "9A", "S5", "SP", "HB9", "LA", "EI", "GW",
               "ZL", "VK", "ZS", "PY", "LU", "CE", "XE", "TI", "HK"]

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
    # Outliers
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
    """Plausible W/K/N call for the given US state — KL7 / KH6 for AK / HI."""
    if state == "AK":
        return f"KL7{''.join(rng.choices('ABCDEFGHIJKLMNOPQRSTUVWXYZ', k=rng.choice([2, 3])))}"
    if state == "HI":
        return f"KH6{''.join(rng.choices('ABCDEFGHIJKLMNOPQRSTUVWXYZ', k=rng.choice([2, 3])))}"
    digit = STATE_TO_AREA_DIGIT.get(state, "0")
    return gen_us_call_with_digit(rng, digit)

def gen_indiana_call(rng):
    """Indiana stations almost always use W9/K9/N9/AA9-AL9 calls."""
    return gen_us_call_with_digit(rng, "9")

def gen_canadian_call(rng, province):
    prefix = rng.choice(CANADIAN_PREFIXES[province])
    suffix_len = rng.choices([2, 3], weights=[60, 40])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{suffix}"

def gen_dx_call(rng):
    prefix = rng.choice(DX_PREFIXES)
    digit = str(rng.randint(0, 9))
    suffix_len = rng.choices([2, 3], weights=[55, 45])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{digit}{suffix}"

def load_inqp_counties():
    with open(CONTEST_FILE, "r") as f:
        contest = json.load(f)
    return contest["validation"]["inStateMults"]

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
    return ("599", "599") if mode == "CW" else ("59", "59")

def insert_dupes(qsos, rng, count, next_id_start, base_ts):
    """Append `count` duplicate QSOs — each one matches an existing QSO's
    callsign + band + mode (per-band/mode dupe rule). Timestamps land 1-3
    minutes after the original so the log makes physical sense."""
    out = []
    next_id = next_id_start
    # Pick from QSOs that aren't themselves dupes
    candidates = [q for q in qsos if not q.get("duplicate", False)]
    for _ in range(count):
        original = rng.choice(candidates)
        # Schedule the dupe a couple minutes after the original.
        orig_ts = datetime.fromisoformat(original["timestamp"].replace("Z", "+00:00"))
        dupe_ts = orig_ts + timedelta(seconds=rng.randint(60, 240))
        # The exchange and call are identical (same station, same band/mode);
        # only id and timestamp differ. The engine's dupe detector should
        # flag it, score 0 points, and not credit a multiplier.
        dupe = make_qso(
            qso_id=next_id,
            ts=dupe_ts,
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

def gen_qsos_for_instate(starter, rng, counties, target_total=110, dupe_count=2):
    """KX9IO from INBEN works everyone. Mix:
       ~30% other IN stations (5-letter county codes, not own county),
       ~45% non-IN US (2-letter state codes),
       ~15% Canadian (2-letter province codes),
       ~10% DX ("DX" — points only, no mult).
    """
    out = []
    next_id = starter["qsos"][-1]["id"] + 1
    last_ts = datetime.fromisoformat(starter["qsos"][-1]["timestamp"].replace("Z", "+00:00"))
    used_keys = set()
    for q in starter["qsos"]:
        used_keys.add((q["callsign"], q["band"],
                       "SSB" if q["mode"] in ("USB", "LSB") else q["mode"]))

    needed_real = target_total - len(starter["qsos"]) - dupe_count
    operator_exch_sent = "INBEN"
    own_county = "INBEN"

    for _ in range(needed_real):
        # 12-hour contest, ~110 QSOs → average ~6.5 minutes between QSOs.
        last_ts = last_ts + timedelta(seconds=rng.randint(150, 600))
        mode = pick_mode(rng)
        band = pick_band_for_mode(rng, mode)
        log_mode = cw_or_ssb_log_mode(mode, band)
        freq = BAND_FREQ_PRESETS[band][mode]

        cat_roll = rng.random()
        if cat_roll < 0.30:
            # Other IN station — sends 5-letter <county> code (skip own).
            their_county = rng.choice(counties)
            while their_county == own_county:
                their_county = rng.choice(counties)
            exch_r = their_county
            call = gen_indiana_call(rng)
        elif cat_roll < 0.75:
            # Non-IN US — 2-letter state. Skip IN itself (Indiana stations
            # only use 5-letter county codes; "IN" 2-letter never appears
            # on-air as someone else's exchange).
            their_state = rng.choice(NON_IN_US_STATES)
            exch_r = their_state
            call = gen_call_for_us_state(rng, their_state)
        elif cat_roll < 0.90:
            province = rng.choice(PROVINCES)
            exch_r = province
            call = gen_canadian_call(rng, province)
        else:
            # DX — points only for IN stations, no mult credit.
            exch_r = "DX"
            call = gen_dx_call(rng)

        # Avoid per-band/mode dupes from random reuse of the same callsign.
        for _ in range(20):
            if (call, band, mode) not in used_keys:
                break
            if exch_r == "DX":
                call = gen_dx_call(rng)
            elif len(exch_r) == 5:
                call = gen_indiana_call(rng)
            elif exch_r in PROVINCES:
                call = gen_canadian_call(rng, exch_r)
            else:
                call = gen_call_for_us_state(rng, exch_r)
        used_keys.add((call, band, mode))

        rst_set = rst_for_mode(mode)
        out.append(make_qso(next_id, last_ts, call, band, log_mode, freq,
                            exch_r, operator_exch_sent, rst_set))
        next_id += 1

    # Append intentional duplicates.
    out += insert_dupes(out + starter["qsos"], rng, dupe_count, next_id, last_ts)
    return out

def gen_qsos_for_outofstate(starter, rng, counties, target_total=110, dupe_count=2):
    """N9OH (FL) works only IN stations. Every received exchange is a
       5-letter IN county code; calls are W9/K9/N9 area-9 calls."""
    out = []
    next_id = starter["qsos"][-1]["id"] + 1
    last_ts = datetime.fromisoformat(starter["qsos"][-1]["timestamp"].replace("Z", "+00:00"))
    used_keys = set()
    for q in starter["qsos"]:
        used_keys.add((q["callsign"], q["band"],
                       "SSB" if q["mode"] in ("USB", "LSB") else q["mode"]))

    needed_real = target_total - len(starter["qsos"]) - dupe_count
    operator_exch_sent = "FL"

    for _ in range(needed_real):
        last_ts = last_ts + timedelta(seconds=rng.randint(150, 600))
        mode = pick_mode(rng)
        band = pick_band_for_mode(rng, mode)
        log_mode = cw_or_ssb_log_mode(mode, band)
        freq = BAND_FREQ_PRESETS[band][mode]

        their_county = rng.choice(counties)
        exch_r = their_county
        call = gen_indiana_call(rng)

        for _ in range(20):
            if (call, band, mode) not in used_keys:
                break
            call = gen_indiana_call(rng)
        used_keys.add((call, band, mode))

        rst_set = rst_for_mode(mode)
        out.append(make_qso(next_id, last_ts, call, band, log_mode, freq,
                            exch_r, operator_exch_sent, rst_set))
        next_id += 1

    out += insert_dupes(out + starter["qsos"], rng, dupe_count, next_id, last_ts)
    return out

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=11,
                        help="RNG seed for reproducible output (default 11)")
    parser.add_argument("--total", type=int, default=110,
                        help="Target total QSOs per file (default 110)")
    parser.add_argument("--dupes", type=int, default=2,
                        help="Number of intentional dupes per file (default 2)")
    args = parser.parse_args()

    counties = load_inqp_counties()

    for path, gen_fn, label in [
        (INSTATE_LOG, gen_qsos_for_instate,    "in-state (IN)"),
        (OOS_LOG,     gen_qsos_for_outofstate, "out-of-state (FL)"),
    ]:
        with open(path, "r") as f:
            log = json.load(f)
        rng = random.Random(args.seed)
        new_qsos = gen_fn(log, rng, counties,
                          target_total=args.total, dupe_count=args.dupes)
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
        print(f"{label}: now {len(log['qsos'])} QSOs ({len(new_qsos)} added, "
              f"{args.dupes} flagged as duplicates) -> {path}")

if __name__ == "__main__":
    main()
