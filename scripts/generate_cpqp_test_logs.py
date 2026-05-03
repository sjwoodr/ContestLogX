#!/usr/bin/env python3
"""
Generate ~115 QSOs each into CPQP test log files.

Reads:  contests/cpqp.json (multiplier list)
Writes: test_logs/test_cpqp_inprairie.clx     (VE5 op, RGW)
        test_logs/test_cpqp_outprairie.clx    (W4 FL op)

Both logs are written from scratch — no starter QSOs.

Usage:  python3 scripts/generate_cpqp_test_logs.py [--seed 23]
"""

import argparse
import json
import os
import random
from datetime import datetime, timedelta, timezone

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTEST_FILE = os.path.join(REPO_ROOT, "contests", "cpqp.json")
INPRAIRIE_LOG  = os.path.join(REPO_ROOT, "test_logs", "test_cpqp_inprairie.clx")
OUTPRAIRIE_LOG = os.path.join(REPO_ROOT, "test_logs", "test_cpqp_outprairie.clx")

# 4 HF bands — CPQP excludes 80m and 160m. Suggested freqs from rules.
BAND_FREQ_PRESETS = {
    "40m":  {"CW": 7035, "SSB": 7220},
    "20m":  {"CW": 14040, "SSB": 14240},
    "15m":  {"CW": 21035, "SSB": 21340},
    "10m":  {"CW": 28035, "SSB": 28340},
}

def ssb_mode_for_band(band):
    return "LSB" if band in ("80m", "40m") else "USB"

# Mode/band weighting matched to typical CPQP activity. 20/40m carry most.
BAND_WEIGHTS_BY_MODE = {
    "CW":  {"40m": 35, "20m": 35, "15m": 20, "10m": 10},
    "SSB": {"40m": 32, "20m": 38, "15m": 20, "10m": 10},
}

# Mode mix — CW + SSB only, no digital per the rules.
MODE_MIX_TARGET = {"CW": 50, "SSB": 50}

# Non-prairie US states (50). The 50-state list — no DC since DC is its
# own thing handled via the namedMultAliases (DC->MD).
US_STATES = [
    "AL", "AK", "AZ", "AR", "CA", "CO", "CT", "DE", "FL", "GA",
    "HI", "ID", "IL", "IN", "IA", "KS", "KY", "LA", "ME", "MD",
    "MA", "MI", "MN", "MS", "MO", "MT", "NE", "NV", "NH", "NJ",
    "NM", "NY", "NC", "ND", "OH", "OK", "OR", "PA", "RI", "SC",
    "SD", "TN", "TX", "UT", "VT", "VA", "WA", "WV", "WI", "WY",
]

# Non-prairie Canadian provinces (10) — the OUTSIDE chart entries.
NON_PRAIRIE_PROVINCES = ["BC", "ON", "QC", "NT", "NU", "YT", "NB", "NL", "NS", "PE"]

# Prairie callsign prefixes by province.
PRAIRIE_CALL_PREFIXES = {
    "MB": ["VE4", "VA4"],
    "SK": ["VE5", "VA5"],
    "AB": ["VE6", "VA6"],
}

CANADIAN_NONPRAIRIE_PREFIXES = {
    "BC": ["VE7", "VA7"],
    "ON": ["VE3", "VA3"],
    "QC": ["VE2", "VA2"],
    "NB": ["VE9", "VA9"],
    "NS": ["VE1", "VA1"],
    "PE": ["VY2"],
    "NL": ["VO1", "VO2"],
    "YT": ["VY1"],
    "NU": ["VY0"],
    "NT": ["VE8", "VA8"],
}

US_PREFIXES_SINGLE = ["W", "K", "N"]
US_PREFIXES_DOUBLE = ["AA", "KA", "KB", "KC", "KE", "KK", "NA", "NF",
                       "WA", "WB", "WD", "WO"]

# State -> US area digit mapping.
STATE_TO_AREA_DIGIT = {
    "CT": "1", "ME": "1", "MA": "1", "NH": "1", "RI": "1", "VT": "1",
    "NY": "2", "NJ": "2",
    "DE": "3", "MD": "3", "PA": "3",
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

# DX prefixes — same curated 1:1 prefix→DXCC list used by NEQP.
DX_PREFIXES = ["G", "DL", "F", "I", "JA", "PA", "OE", "SM", "OZ", "OK",
               "SP", "LA", "EI", "LU", "XE"]


def gen_us_call_with_digit(rng, digit):
    if rng.random() < 0.7:
        prefix = rng.choice(US_PREFIXES_SINGLE)
    else:
        prefix = rng.choice(US_PREFIXES_DOUBLE)
    suffix_len = rng.choices([2, 3], weights=[55, 45])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{digit}{suffix}"


def gen_us_call(rng, state):
    if state == "AK":
        return f"KL7{''.join(rng.choices('ABCDEFGHIJKLMNOPQRSTUVWXYZ', k=rng.choice([2, 3])))}"
    if state == "HI":
        return f"KH6{''.join(rng.choices('ABCDEFGHIJKLMNOPQRSTUVWXYZ', k=rng.choice([2, 3])))}"
    digit = STATE_TO_AREA_DIGIT.get(state, "0")
    return gen_us_call_with_digit(rng, digit)


def gen_canadian_nonprairie_call(rng, province):
    prefix = rng.choice(CANADIAN_NONPRAIRIE_PREFIXES[province])
    suffix_len = rng.choices([2, 3], weights=[60, 40])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{suffix}"


def gen_prairie_call(rng, province):
    """Generate a callsign for the given prairie province."""
    prefix = rng.choice(PRAIRIE_CALL_PREFIXES[province])
    suffix_len = rng.choices([2, 3], weights=[50, 50])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{suffix}"


def gen_dx_call(rng):
    prefix = rng.choice(DX_PREFIXES)
    digit = str(rng.randint(0, 9))
    suffix_len = rng.choices([2, 3], weights=[55, 45])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{digit}{suffix}"


def load_cpqp_data():
    with open(CONTEST_FILE, "r") as f:
        contest = json.load(f)
    feds = contest["validation"]["inStateMults"]
    # Map FED -> province via multAliases entries.
    fed_to_prov = {}
    for alias in contest["validation"]["multAliases"]:
        prov = alias["mapsTo"]
        for fed in alias["sourceValues"]:
            fed_to_prov[fed] = prov
    return feds, fed_to_prov


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
    return ssb_mode_for_band(band)


def make_qso(qso_id, ts, callsign, band, mode_log, freq, exch_recvd, exch_sent,
             rst_set, duplicate=False):
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


def build_log_skeleton(operator_call, station_type, my_exchange,
                       operator_state="", description=""):
    return {
        "contest": {
            "categories": {
                "userPrompt_myExchange": my_exchange,
                "userPrompt_operatingClass": "SO_LP",
                "userPrompt_stationType": station_type,
            },
            "contest_file": "cpqp.json",
            "mode": "",
            "name": "Canadian Prairies QSO Party",
            "type": "Canadian Prairies QSO Party",
            "version": "1.0.0",
            "year": 2026
        },
        "created": "2026-05-09T17:00:00",
        "exchange_fields": [],
        "format": "ContestLogX",
        "format_version": "1.0",
        "metadata": {
            "callsign": operator_call,
            "club": "",
            "name": "Steve",
            "qth": operator_state,
        },
        "modified": "",
        "qsos": [],
        "statistics": {"score": 0, "total_points": 0, "total_qsos": 0},
    }


def gen_inprairie(rng, feds, fed_to_prov, target_total=115, dupe_count=3):
    """VE5KAT operating from RGW (Regina Wascana, SK). CP station — works
    everyone. Mix:
      ~25% prairie (FED received, aliased to MB/SK/AB),
      ~50% US (state mults),
      ~15% non-prairie Canadian (province mults),
      ~10% DX (1 point, no mult).
    """
    log = build_log_skeleton("VE5KAT", "CP", "RGW", operator_state="SK",
                              description="In-prairie SK operator")
    qsos = []
    next_id = 1
    last_ts = datetime(2026, 5, 9, 17, 0, 0)
    used_keys = set()
    needed_real = target_total - dupe_count
    operator_exch_sent = "RGW"

    for _ in range(needed_real):
        # 10-hour window, ~115 QSOs => ~5 minutes apart on average.
        last_ts = last_ts + timedelta(seconds=rng.randint(120, 400))
        mode = pick_mode(rng)
        band = pick_band_for_mode(rng, mode)
        log_mode = cw_or_ssb_log_mode(mode, band)
        freq = BAND_FREQ_PRESETS[band][mode]

        cat_roll = rng.random()
        if cat_roll < 0.25:
            # Other prairie station — pick any province, then any FED.
            prov = rng.choices(["MB", "SK", "AB"], weights=[14, 14, 37])[0]
            their_fed = rng.choice([f for f, p in fed_to_prov.items() if p == prov])
            # Avoid sending own FED (RGW) when the worked op is in SK.
            while their_fed == operator_exch_sent:
                their_fed = rng.choice([f for f, p in fed_to_prov.items() if p == prov])
            exch_r = their_fed
            call = gen_prairie_call(rng, prov)
        elif cat_roll < 0.75:
            their_state = rng.choice(US_STATES)
            exch_r = their_state
            call = gen_us_call(rng, their_state)
        elif cat_roll < 0.90:
            province = rng.choice(NON_PRAIRIE_PROVINCES)
            exch_r = province
            call = gen_canadian_nonprairie_call(rng, province)
        else:
            exch_r = "DX"
            call = gen_dx_call(rng)

        for _ in range(20):
            if (call, band, mode) not in used_keys:
                break
            if exch_r == "DX":
                call = gen_dx_call(rng)
            elif len(exch_r) == 3:
                call = gen_prairie_call(rng, fed_to_prov[exch_r])
            elif exch_r in NON_PRAIRIE_PROVINCES:
                call = gen_canadian_nonprairie_call(rng, exch_r)
            else:
                call = gen_us_call(rng, exch_r)
        used_keys.add((call, band, mode))

        rst_set = rst_for_mode(mode)
        qsos.append(make_qso(next_id, last_ts, call, band, log_mode, freq,
                              exch_r, operator_exch_sent, rst_set))
        next_id += 1

    qsos += insert_dupes(qsos, rng, dupe_count, next_id)
    log["qsos"] = qsos
    return log


def gen_outprairie(rng, feds, fed_to_prov, target_total=115, dupe_count=2):
    """K4XYZ operating from FL. Non-prairie — works only prairie stations."""
    log = build_log_skeleton("K4XYZ", "OUTSIDE", "FL", operator_state="FL",
                              description="Out-of-prairie FL operator")
    qsos = []
    next_id = 1
    last_ts = datetime(2026, 5, 9, 17, 0, 0)
    used_keys = set()
    needed_real = target_total - dupe_count
    operator_exch_sent = "FL"

    for _ in range(needed_real):
        last_ts = last_ts + timedelta(seconds=rng.randint(120, 400))
        mode = pick_mode(rng)
        band = pick_band_for_mode(rng, mode)
        log_mode = cw_or_ssb_log_mode(mode, band)
        freq = BAND_FREQ_PRESETS[band][mode]

        # Pick a prairie province first (weighted by FED count), then a FED in it.
        prov = rng.choices(["MB", "SK", "AB"], weights=[14, 14, 37])[0]
        their_fed = rng.choice([f for f, p in fed_to_prov.items() if p == prov])
        exch_r = their_fed
        call = gen_prairie_call(rng, prov)

        for _ in range(20):
            if (call, band, mode) not in used_keys:
                break
            call = gen_prairie_call(rng, prov)
        used_keys.add((call, band, mode))

        rst_set = rst_for_mode(mode)
        qsos.append(make_qso(next_id, last_ts, call, band, log_mode, freq,
                              exch_r, operator_exch_sent, rst_set))
        next_id += 1

    qsos += insert_dupes(qsos, rng, dupe_count, next_id)
    log["qsos"] = qsos
    return log


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=23,
                        help="RNG seed for reproducibility (default 23)")
    parser.add_argument("--total", type=int, default=115,
                        help="Target total QSOs per file (default 115)")
    args = parser.parse_args()

    feds, fed_to_prov = load_cpqp_data()

    for path, gen_fn, label in [
        (INPRAIRIE_LOG,  gen_inprairie,  "in-prairie SK (CP)"),
        (OUTPRAIRIE_LOG, gen_outprairie, "out-of-prairie FL"),
    ]:
        rng = random.Random(args.seed)
        actual_dupes = rng.choice([1, 2, 3, 4])
        rng = random.Random(args.seed)
        log = gen_fn(rng, feds, fed_to_prov,
                     target_total=args.total, dupe_count=actual_dupes)
        log["statistics"]["total_qsos"] = len(log["qsos"])
        log["modified"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S")
        with open(path, "w") as f:
            json.dump(log, f, indent=4)
            f.write("\n")
        print(f"{label}: {len(log['qsos'])} QSOs ({actual_dupes} dupes) -> {path}")


if __name__ == "__main__":
    main()
