#!/usr/bin/env python3
"""
Generate ARQP test logs from scratch.

Reads:  contests/arqp.json (multiplier list)
Writes: test_logs/test_arqp_inar.clx        (AR op from Pulaski, PUL)
        test_logs/test_arqp_outar.clx       (MA op working AR stations only)

Usage:  python3 scripts/generate_arqp_test_logs.py [--seed 23]
"""

import argparse
import json
import os
import random
from datetime import datetime, timedelta, timezone

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTEST_FILE = os.path.join(REPO_ROOT, "contests", "arqp.json")
INAR_LOG  = os.path.join(REPO_ROOT, "test_logs", "test_arqp_inar.clx")
OUTAR_LOG = os.path.join(REPO_ROOT, "test_logs", "test_arqp_outar.clx")

# 8 bands — ARQP includes 6m and 2m. Suggested freqs from rules.
BAND_FREQ_PRESETS = {
    "160m": {"CW": 1830, "SSB": 1870, "DIGITAL": 1840},
    "80m":  {"CW": 3540, "SSB": 3820, "DIGITAL": 3580},
    "40m":  {"CW": 7040, "SSB": 7200, "DIGITAL": 7080},
    "20m":  {"CW": 14040, "SSB": 14260, "DIGITAL": 14080},
    "15m":  {"CW": 21040, "SSB": 21360, "DIGITAL": 21080},
    "10m":  {"CW": 28040, "SSB": 28360, "DIGITAL": 28080},
    "6m":   {"CW": 50090, "SSB": 50360, "DIGITAL": 50300},
    "2m":   {"CW": 144050, "SSB": 144200, "DIGITAL": 144150},
}

def ssb_mode_for_band(band):
    return "LSB" if band in ("160m", "80m", "40m") else "USB"

# Mode/band weighting — HF dominates, VHF appears occasionally.
BAND_WEIGHTS_BY_MODE = {
    "CW":      {"160m": 5, "80m": 18, "40m": 30, "20m": 28, "15m": 12, "10m": 5, "6m": 2, "2m": 0},
    "SSB":     {"160m": 3, "80m": 15, "40m": 28, "20m": 32, "15m": 13, "10m": 6, "6m": 2, "2m": 1},
    "DIGITAL": {"160m": 0, "80m": 12, "40m": 30, "20m": 45, "15m": 10, "10m": 3, "6m": 0, "2m": 0},
}

MODE_MIX_TARGET = {"CW": 45, "SSB": 50, "DIGITAL": 5}

# Non-AR US states (49) — Arkansas excluded since it's the host state.
NON_AR_STATES = [
    "AL", "AK", "AZ", "CA", "CO", "CT", "DE", "FL", "GA",
    "HI", "ID", "IL", "IN", "IA", "KS", "KY", "LA", "ME", "MD",
    "MA", "MI", "MN", "MS", "MO", "MT", "NE", "NV", "NH", "NJ",
    "NM", "NY", "NC", "ND", "OH", "OK", "OR", "PA", "RI", "SC",
    "SD", "TN", "TX", "UT", "VT", "VA", "WA", "WV", "WI", "WY",
]

CANADIAN_PROVINCES = ["AB", "BC", "MB", "NB", "NL", "NT", "NS", "NU", "ON", "PE", "QC", "SK", "YT"]

CANADIAN_PREFIXES = {
    "AB":  ["VE6", "VA6"], "BC":  ["VE7", "VA7"], "MB":  ["VE4", "VA4"],
    "NB":  ["VE9", "VA9"], "NL":  ["VO1", "VO2"], "NT":  ["VE8", "VA8"],
    "NS":  ["VE1", "VA1"], "NU":  ["VY0"],        "ON":  ["VE3", "VA3"],
    "PE":  ["VY2"],        "QC":  ["VE2", "VA2"], "SK":  ["VE5", "VA5"],
    "YT":  ["VY1"],
}

US_PREFIXES_SINGLE = ["W", "K", "N"]
US_PREFIXES_DOUBLE = ["AA", "KA", "KB", "KC", "KE", "KK", "NA", "NF",
                       "WA", "WB", "WD", "WO"]

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

# DX prefixes — 14-prefix curated list, each maps 1:1 to a single DXCC entity.
DX_PREFIXES = ["G", "DL", "F", "I", "JA", "PA", "OE", "SM", "OZ", "OK",
               "SP", "LA", "EI", "LU", "XE"]


def gen_us_call(rng, state):
    if state == "AK":
        return f"KL7{''.join(rng.choices('ABCDEFGHIJKLMNOPQRSTUVWXYZ', k=rng.choice([2, 3])))}"
    if state == "HI":
        return f"KH6{''.join(rng.choices('ABCDEFGHIJKLMNOPQRSTUVWXYZ', k=rng.choice([2, 3])))}"
    if rng.random() < 0.7:
        prefix = rng.choice(US_PREFIXES_SINGLE)
    else:
        prefix = rng.choice(US_PREFIXES_DOUBLE)
    digit = STATE_TO_AREA_DIGIT.get(state, "0")
    suffix_len = rng.choices([2, 3], weights=[55, 45])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{digit}{suffix}"


def gen_ar_call(rng):
    """Arkansas callsign — area-5."""
    if rng.random() < 0.7:
        prefix = rng.choice(US_PREFIXES_SINGLE)
    else:
        prefix = rng.choice(US_PREFIXES_DOUBLE)
    suffix_len = rng.choices([2, 3], weights=[55, 45])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}5{suffix}"


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


def load_arqp_data():
    with open(CONTEST_FILE, "r") as f:
        contest = json.load(f)
    counties = contest["validation"]["inStateMults"]
    return counties


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
    return "RTTY"


def make_qso(qso_id, ts, callsign, band, mode_log, freq, exch_recvd, exch_sent,
             rst_set, name_recvd="", name_sent="STEVE", duplicate=False):
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
            "NAMEr": name_recvd,
            "NAMEs": name_sent,
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


def build_log_skeleton(operator_call, station_type, my_exchange, op_class,
                       operator_state):
    return {
        "contest": {
            "categories": {
                "userPrompt_myExchange": my_exchange,
                "userPrompt_operatingClass": op_class,
                "userPrompt_stationType": station_type,
            },
            "contest_file": "arqp.json",
            "mode": "",
            "name": "Arkansas QSO Party",
            "type": "Arkansas QSO Party",
            "version": "1.0.0",
            "year": 2026
        },
        "created": "2026-05-16T14:00:00",
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


def gen_inar(rng, counties, target_total=115, dupe_count=3):
    """W5AAR operating from Pulaski (PUL). AR station, single-op LP mixed
    modes. Works everyone:
      ~25% other AR stations (county codes received),
      ~50% non-AR US (state codes — 49 states),
      ~15% Canadian (province codes — 13),
      ~10% DX (always credited as single 'DX' mult).
    """
    log = build_log_skeleton("W5AAR", "AR", "PUL", "SO_LP_MIXED",
                              operator_state="AR")
    qsos = []
    next_id = 1
    last_ts = datetime(2026, 5, 16, 14, 0, 0)
    used_keys = set()
    needed_real = target_total - dupe_count
    own_county = "PUL"
    operator_exch_sent = own_county

    for _ in range(needed_real):
        # 12-hour window, ~115 QSOs => ~6 minutes apart on average.
        last_ts = last_ts + timedelta(seconds=rng.randint(150, 500))
        mode = pick_mode(rng)
        band = pick_band_for_mode(rng, mode)
        log_mode = cw_or_ssb_log_mode(mode, band)
        freq = BAND_FREQ_PRESETS[band][mode]

        cat_roll = rng.random()
        if cat_roll < 0.25:
            # Other AR station — pick any county except own.
            their_county = rng.choice(counties)
            while their_county == own_county:
                their_county = rng.choice(counties)
            exch_r = their_county
            call = gen_ar_call(rng)
        elif cat_roll < 0.75:
            their_state = rng.choice(NON_AR_STATES)
            exch_r = their_state
            call = gen_us_call(rng, their_state)
        elif cat_roll < 0.90:
            province = rng.choice(CANADIAN_PROVINCES)
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
            elif len(exch_r) == 3:
                call = gen_ar_call(rng)
            elif exch_r in CANADIAN_PROVINCES:
                call = gen_canadian_call(rng, exch_r)
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


def gen_outar(rng, counties, target_total=115, dupe_count=2):
    """K1MAS operating from MA. Non-AR — works only AR stations."""
    log = build_log_skeleton("K1MAS", "OUTSIDE", "MA", "SO_LP_MIXED",
                              operator_state="MA")
    qsos = []
    next_id = 1
    last_ts = datetime(2026, 5, 16, 14, 0, 0)
    used_keys = set()
    needed_real = target_total - dupe_count
    operator_exch_sent = "MA"

    for _ in range(needed_real):
        last_ts = last_ts + timedelta(seconds=rng.randint(150, 500))
        mode = pick_mode(rng)
        band = pick_band_for_mode(rng, mode)
        log_mode = cw_or_ssb_log_mode(mode, band)
        freq = BAND_FREQ_PRESETS[band][mode]

        their_county = rng.choice(counties)
        exch_r = their_county
        call = gen_ar_call(rng)

        for _ in range(20):
            if (call, band, mode) not in used_keys:
                break
            call = gen_ar_call(rng)
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

    counties = load_arqp_data()

    for path, gen_fn, label in [
        (INAR_LOG,  gen_inar,  "in-AR PUL"),
        (OUTAR_LOG, gen_outar, "out-of-AR MA"),
    ]:
        rng = random.Random(args.seed)
        actual_dupes = rng.choice([1, 2, 3, 4])
        rng = random.Random(args.seed)
        log = gen_fn(rng, counties,
                     target_total=args.total, dupe_count=actual_dupes)
        log["statistics"]["total_qsos"] = len(log["qsos"])
        log["modified"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S")
        with open(path, "w") as f:
            json.dump(log, f, indent=4)
            f.write("\n")
        print(f"{label}: {len(log['qsos'])} QSOs ({actual_dupes} dupes) -> {path}")


if __name__ == "__main__":
    main()
