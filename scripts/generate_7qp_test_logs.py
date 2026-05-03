#!/usr/bin/env python3
"""
Generate ~100 QSOs each into the starter 7QP test log files. Deterministic via
a fixed RNG seed so the produced logs are stable across re-runs and across
contributors verifying scoring changes.

Reads:  test_logs/test_7qp_instate.clx     (AZ operator, station N9OH)
        test_logs/test_7qp_outofstate.clx  (FL operator, station N9OH)
Writes: same files, with the existing starter QSOs preserved and new
        synthesized QSOs appended.

Usage:  python3 scripts/generate_7qp_test_logs.py [--seed 7]
"""

import argparse
import json
import os
import random
from datetime import datetime, timedelta, timezone

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTEST_FILE = os.path.join(REPO_ROOT, "contests", "7qp.json")
INSTATE_LOG  = os.path.join(REPO_ROOT, "test_logs", "test_7qp_instate.clx")
OOS_LOG      = os.path.join(REPO_ROOT, "test_logs", "test_7qp_outofstate.clx")

# 7QP-specific knobs.
SEVENTH_AREA_STATES = ["AZ", "ID", "MT", "NV", "OR", "UT", "WA", "WY"]

# US states a non-7th-area station might send, minus the 7th-area states.
NON7_US_STATES = [
    "AL", "AR", "CA", "CO", "CT", "DE", "DC", "FL", "GA", "IL",
    "IN", "IA", "KS", "KY", "LA", "ME", "MD", "MA", "MI", "MN",
    "MS", "MO", "NE", "NH", "NJ", "NM", "NY", "NC", "ND", "OH",
    "OK", "PA", "RI", "SC", "SD", "TN", "TX", "VT", "VA", "WV",
    "WI",
    # AK and HI are technically 7th call area territory geographically but
    # use 7th-call indicators (KL7, KH6) — keep them in the non-7 bucket
    # for QSO-party scoring purposes since they aren't part of the 7QP
    # in-area state list.
    "AK", "HI",
]

PROVINCES = ["NS", "QC", "ON", "MB", "SK", "AB", "BC", "NT", "NB", "NL", "NU", "YT", "PE"]

# Band frequency layout — must align with contests/7qp.json frequency ranges
# so the engine accepts each QSO. CW and DIGITAL go in the CW sub-band per
# 7QP rule "All CW and Digital contacts must be in the CW/Data sub-bands."
BAND_FREQ_PRESETS = {
    "160m": {"CW": 1820, "SSB": 1850, "DIGITAL": 1830},
    "80m":  {"CW": 3540, "SSB": 3855, "DIGITAL": 3580},
    "40m":  {"CW": 7035, "SSB": 7180, "DIGITAL": 7080},
    "20m":  {"CW": 14040, "SSB": 14255, "DIGITAL": 14080},
    "15m":  {"CW": 21040, "SSB": 21355, "DIGITAL": 21080},
    "10m":  {"CW": 28040, "SSB": 28455, "DIGITAL": 28080},
}

# CLX stores SSB QSOs as USB or LSB (mode column) but the contest engine
# normalizes both to "SSB" for scoring. We'll use band-typical conventions:
# LSB on 80/40/160, USB above.
def ssb_mode_for_band(band):
    return "LSB" if band in ("160m", "80m", "40m") else "USB"

# Mode → band weighting. CW spread thinner on 160m (rare for casual ops),
# heavier on 20/40 (the contest workhorse bands).
BAND_WEIGHTS_BY_MODE = {
    "CW":      {"160m": 3,  "80m": 12, "40m": 30, "20m": 35, "15m": 15, "10m": 5},
    "SSB":     {"160m": 2,  "80m": 10, "40m": 28, "20m": 38, "15m": 17, "10m": 5},
    "DIGITAL": {"160m": 0,  "80m": 5,  "40m": 25, "20m": 50, "15m": 15, "10m": 5},
}

# Mode mix for ~100 QSOs: target roughly 45% CW + 45% SSB + 8% Digital
# (matches "very few RTTY contacts" per the user's request).
MODE_MIX_TARGET = {"CW": 45, "SSB": 47, "DIGITAL": 8}

# US callsign generator localized to area-digit constraints (so a "7th-area"
# call genuinely uses digit 7 etc.). Keeping it inline rather than importing
# scripts/generate_callsigns.py because we need digit control.
US_PREFIXES_SINGLE = ["W", "K", "N"]
US_PREFIXES_DOUBLE = ["AA", "AB", "AC", "AD", "AE", "AF", "AG", "AI", "AJ", "AK", "KA", "KB", "KC", "KD", "KE", "KF", "KG", "KI", "KJ", "KK", "KM", "KN", "KO", "NA", "NB", "NC", "ND", "NE", "NF", "NG", "NI", "NJ", "NK", "NN", "NO", "WA", "WB", "WD", "WE", "WF", "WG", "WI", "WJ", "WK", "WM", "WN", "WO"]

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

DX_PREFIXES = ["G", "DL", "F", "I", "JA", "EA", "PA", "ON", "OH", "SM", "OZ", "OK", "OE", "EI", "GW", "HB9", "LA", "LX", "LZ", "9A", "S5", "SP", "SV", "TA", "UA3", "UA9", "YL", "YO", "YU", "ZL", "VK", "ZS", "PY", "LU", "CE", "XE", "TI", "HK", "HC"]

def gen_us_call_with_digit(rng, digit):
    """Return a US call whose area-digit is the given character."""
    if rng.random() < 0.7:
        prefix = rng.choice(US_PREFIXES_SINGLE)
    else:
        prefix = rng.choice(US_PREFIXES_DOUBLE)
    suffix_len = rng.choices([1, 2, 3], weights=[5, 50, 45])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{digit}{suffix}"

# Mapping from non-7 US state to a typical W-area digit. CLX doesn't enforce
# this and operators can be anywhere, but plausible test data helps the
# generated logs look like real on-air activity. Roughly aligns with the
# canonical W call areas.
STATE_TO_AREA_DIGIT = {
    # W1
    "CT": "1", "ME": "1", "MA": "1", "NH": "1", "RI": "1", "VT": "1",
    # W2
    "NY": "2", "NJ": "2",
    # W3
    "DE": "3", "MD": "3", "PA": "3", "DC": "3",
    # W4
    "AL": "4", "FL": "4", "GA": "4", "KY": "4", "NC": "4", "SC": "4", "TN": "4", "VA": "4",
    # W5
    "AR": "5", "LA": "5", "MS": "5", "NM": "5", "OK": "5", "TX": "5",
    # W6
    "CA": "6",
    # W7 — 7th-area states (handled separately)
    # W8
    "MI": "8", "OH": "8", "WV": "8",
    # W9
    "IL": "9", "IN": "9", "WI": "9",
    # W0
    "CO": "0", "IA": "0", "KS": "0", "MN": "0", "MO": "0", "NE": "0", "ND": "0", "SD": "0",
    # Outliers
    "AK": "L", "HI": "H",  # KL7 / KH6 — handled separately below
}

def gen_call_for_us_state(rng, state):
    """Plausible W/K/N call for the given US state."""
    if state == "AK":
        return f"KL7{''.join(rng.choices('ABCDEFGHIJKLMNOPQRSTUVWXYZ', k=rng.choice([2, 3])))}"
    if state == "HI":
        return f"KH6{''.join(rng.choices('ABCDEFGHIJKLMNOPQRSTUVWXYZ', k=rng.choice([2, 3])))}"
    digit = STATE_TO_AREA_DIGIT.get(state, "0")
    return gen_us_call_with_digit(rng, digit)

def gen_call_for_7th_area(rng):
    """7th-area call (W7/K7/N7/AA7-AL7 etc.)."""
    return gen_us_call_with_digit(rng, "7")

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

def load_county_codes_by_state():
    """Group the 7QP inStateMults entries by their 2-letter state prefix."""
    with open(CONTEST_FILE, "r") as f:
        contest = json.load(f)
    counties = contest["validation"]["inStateMults"]
    by_state = {s: [] for s in SEVENTH_AREA_STATES}
    for code in counties:
        prefix = code[:2]
        if prefix in by_state:
            by_state[prefix].append(code)
    return by_state

def pick_mode(rng):
    """Weighted mode selection using MODE_MIX_TARGET."""
    modes = list(MODE_MIX_TARGET.keys())
    weights = list(MODE_MIX_TARGET.values())
    return rng.choices(modes, weights=weights)[0]

def pick_band_for_mode(rng, mode):
    weights = BAND_WEIGHTS_BY_MODE[mode]
    bands = list(weights.keys())
    return rng.choices(bands, weights=[weights[b] for b in bands])[0]

def cw_or_ssb_log_mode(mode, band):
    """The MODE column value stored in the .clx (CW, USB, LSB, RTTY)."""
    if mode == "CW":
        return "CW"
    if mode == "SSB":
        return ssb_mode_for_band(band)
    if mode == "DIGITAL":
        return "RTTY"
    return mode

def make_qso(qso_id, ts, callsign, band, mode_log, freq, exch_recvd, exch_sent, rst_set):
    """Build one QSO dict in the .clx format the engine consumes."""
    rst_s, rst_r = rst_set
    return {
        "band": band,
        "callsign": callsign,
        "duplicate": False,
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
        "multiplier_count": 0,   # engine recalculates on load
        "points":           0,   # engine recalculates on load
        "rst_received": rst_r,
        "rst_sent":     rst_s,
        "serial": qso_id,
        "timestamp": ts.strftime("%Y-%m-%dT%H:%M:%SZ"),
    }

def rst_for_mode(mode):
    return ("599", "599") if mode in ("CW", "DIGITAL") else ("59", "59")

def gen_qsos_for_instate(starter, rng, county_by_state, target_total=100):
    """
    AZ operator, sends AZMCP. Works EVERYONE.
    Mix: ~25% other 7th-area (5-letter codes) — alias collapses to state mult,
         ~50% other US W/VE non-7th-area (2-letter codes),
         ~15% Canadian provinces, ~10% DX.
    """
    out = []
    next_id = starter["qsos"][-1]["id"] + 1
    last_ts = datetime.fromisoformat(starter["qsos"][-1]["timestamp"].replace("Z", "+00:00"))
    used_calls_per_band_mode = set()

    # Track first-saved-qsos to ensure they don't dupe with new ones
    for qso in starter["qsos"]:
        log_mode = qso["mode"]
        norm_mode = "SSB" if log_mode in ("USB", "LSB") else ("DIGITAL" if log_mode in ("RTTY", "FT8", "FT4") else log_mode)
        used_calls_per_band_mode.add((qso["callsign"], qso["band"], norm_mode))

    needed = target_total - len(starter["qsos"])
    operator_exch_sent = "AZMCP"

    for _ in range(needed):
        # Time stride: average ~9.5 minutes between QSOs to span the 18-hour
        # contest comfortably (98 QSOs × 10 minutes = 16.3 hours).
        last_ts = last_ts + timedelta(seconds=rng.randint(120, 900))

        mode = pick_mode(rng)            # CW / SSB / DIGITAL
        band = pick_band_for_mode(rng, mode)
        log_mode = cw_or_ssb_log_mode(mode, band)
        freq = BAND_FREQ_PRESETS[band][mode]

        # Pick contact category.
        cat_roll = rng.random()
        if cat_roll < 0.25:
            # Other 7th-area station — sends 5-letter <state><county>
            their_state = rng.choice(SEVENTH_AREA_STATES)
            # Skip the operator's own state for variety on multipliers
            while their_state == "AZ":
                their_state = rng.choice(SEVENTH_AREA_STATES)
            exch_r = rng.choice(county_by_state[their_state])
            call = gen_call_for_7th_area(rng)
        elif cat_roll < 0.75:
            # Non-7 W station
            their_state = rng.choice(NON7_US_STATES)
            exch_r = their_state
            call = gen_call_for_us_state(rng, their_state)
        elif cat_roll < 0.90:
            # Canadian
            province = rng.choice(PROVINCES)
            exch_r = province
            call = gen_canadian_call(rng, province)
        else:
            # DX
            exch_r = "DX"
            call = gen_dx_call(rng)

        # Avoid per-band/mode dupes: if we hit one, regenerate the call with
        # the SAME exchange (the operator works a different person from the
        # same place). Cap retries so we always make progress.
        for _ in range(20):
            if (call, band, mode) not in used_calls_per_band_mode:
                break
            if exch_r == "DX":
                call = gen_dx_call(rng)
            elif len(exch_r) == 5:
                call = gen_call_for_7th_area(rng)
            elif exch_r in PROVINCES:
                call = gen_canadian_call(rng, exch_r)
            else:
                call = gen_call_for_us_state(rng, exch_r)
        used_calls_per_band_mode.add((call, band, mode))

        rst_set = rst_for_mode(mode)
        out.append(make_qso(next_id, last_ts, call, band, log_mode, freq, exch_r, operator_exch_sent, rst_set))
        next_id += 1
    return out

def gen_qsos_for_outofstate(starter, rng, county_by_state, target_total=100):
    """
    FL operator, sends FL. Works ONLY 7th-area stations.
    All received exchanges are 5-letter <state><county> codes; calls are
    7th-area W/K/N calls.
    """
    out = []
    next_id = starter["qsos"][-1]["id"] + 1
    last_ts = datetime.fromisoformat(starter["qsos"][-1]["timestamp"].replace("Z", "+00:00"))
    used_calls_per_band_mode = set()

    for qso in starter["qsos"]:
        log_mode = qso["mode"]
        norm_mode = "SSB" if log_mode in ("USB", "LSB") else ("DIGITAL" if log_mode in ("RTTY", "FT8", "FT4") else log_mode)
        used_calls_per_band_mode.add((qso["callsign"], qso["band"], norm_mode))

    needed = target_total - len(starter["qsos"])
    operator_exch_sent = "FL"

    for _ in range(needed):
        last_ts = last_ts + timedelta(seconds=rng.randint(120, 900))

        mode = pick_mode(rng)
        band = pick_band_for_mode(rng, mode)
        log_mode = cw_or_ssb_log_mode(mode, band)
        freq = BAND_FREQ_PRESETS[band][mode]

        their_state = rng.choice(SEVENTH_AREA_STATES)
        exch_r = rng.choice(county_by_state[their_state])
        call = gen_call_for_7th_area(rng)

        for _ in range(20):
            if (call, band, mode) not in used_calls_per_band_mode:
                break
            call = gen_call_for_7th_area(rng)
        used_calls_per_band_mode.add((call, band, mode))

        rst_set = rst_for_mode(mode)
        out.append(make_qso(next_id, last_ts, call, band, log_mode, freq, exch_r, operator_exch_sent, rst_set))
        next_id += 1
    return out

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=7,
                        help="RNG seed for reproducible output (default 7)")
    parser.add_argument("--total", type=int, default=100,
                        help="Target total QSOs per file (default 100)")
    args = parser.parse_args()

    county_by_state = load_county_codes_by_state()

    for path, gen_fn, label in [
        (INSTATE_LOG, gen_qsos_for_instate, "in-state (AZ)"),
        (OOS_LOG,    gen_qsos_for_outofstate, "out-of-state (FL)"),
    ]:
        with open(path, "r") as f:
            log = json.load(f)
        rng = random.Random(args.seed)
        new_qsos = gen_fn(log, rng, county_by_state, target_total=args.total)
        log["qsos"].extend(new_qsos)
        # Update statistics (placeholders — engine recalculates on load).
        log["statistics"] = {
            "score": 0,
            "total_points": 0,
            "total_qsos": len(log["qsos"]),
        }
        log["modified"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S")
        with open(path, "w") as f:
            json.dump(log, f, indent=4)
            f.write("\n")
        print(f"{label}: now {len(log['qsos'])} QSOs ({len(new_qsos)} added) -> {path}")

if __name__ == "__main__":
    main()
