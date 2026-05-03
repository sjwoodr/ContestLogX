#!/usr/bin/env python3
"""
Generate UN DX Contest test logs from scratch.

Reads:  contests/undx.json (KDA district list)
Writes: test_logs/test_undx_un.clx     (Kazakhstan op from Almaty, G01)
        test_logs/test_undx_dx.clx     (USA op from FL)

Usage:  python3 scripts/generate_undx_test_logs.py [--seed 23]
"""

import argparse
import json
import os
import random
from datetime import datetime, timedelta, timezone

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTEST_FILE = os.path.join(REPO_ROOT, "contests", "undx.json")
UN_LOG = os.path.join(REPO_ROOT, "test_logs", "test_undx_un.clx")
DX_LOG = os.path.join(REPO_ROOT, "test_logs", "test_undx_dx.clx")

BAND_FREQ_PRESETS = {
    "80m":  {"CW": 3540, "SSB": 3700},
    "40m":  {"CW": 7020, "SSB": 7100},
    "20m":  {"CW": 14040, "SSB": 14250},
    "15m":  {"CW": 21040, "SSB": 21300},
    "10m":  {"CW": 28040, "SSB": 28500},
}

def ssb_mode_for_band(band):
    return "LSB" if band in ("80m", "40m") else "USB"

BAND_WEIGHTS_BY_MODE = {
    "CW":  {"80m": 12, "40m": 30, "20m": 35, "15m": 17, "10m": 6},
    "SSB": {"80m": 10, "40m": 28, "20m": 38, "15m": 18, "10m": 6},
}

MODE_MIX_TARGET = {"CW": 50, "SSB": 50}

# DX prefixes — 14-prefix curated list (each maps 1:1 to a single DXCC entity).
DX_PREFIXES = ["G", "DL", "F", "I", "JA", "PA", "OE", "SM", "OZ", "OK",
               "SP", "LA", "EI", "LU", "XE"]

# US states for the USA op test log (callsign generation only).
US_PREFIXES = ["W", "K", "N"]


def gen_un_call(rng):
    """Generate a Kazakhstan callsign — UN or UP prefix, area 0-9."""
    prefix = rng.choice(["UN", "UP"])
    digit = str(rng.randint(0, 9))
    suffix_len = rng.choices([2, 3], weights=[60, 40])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{digit}{suffix}"


def gen_us_call(rng):
    prefix = rng.choice(US_PREFIXES)
    digit = str(rng.randint(0, 9))
    suffix_len = rng.choices([2, 3], weights=[55, 45])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{digit}{suffix}"


def gen_dx_call(rng):
    prefix = rng.choice(DX_PREFIXES)
    digit = str(rng.randint(0, 9))
    suffix_len = rng.choices([2, 3], weights=[55, 45])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{digit}{suffix}"


def load_undx_data():
    with open(CONTEST_FILE, "r") as f:
        contest = json.load(f)
    return contest["validation"]["namedMults"]


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
            "RSTr":  rst_r,
            "RSTs":  rst_s,
            "SNs":   str(qso_id),
        },
        "frequency": freq,
        "grid_square_count": 0,
        "id": qso_id,
        "mode": mode_log,
        "multiplier_count": 0,
        "points": 0,
        "rst_received": rst_r,
        "rst_sent": rst_s,
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


def build_skeleton(operator_call, station_type, my_exchange, op_class,
                   state="", grid="", cq_zone=0, itu_zone=0):
    """Build CLX file skeleton with a proper `station` block (ClxFile parses
    `station`, not `metadata`). Without the right block, the engine falls back
    to the user's settings callsign — which makes scoring non-deterministic
    across machines and breaks tests that depend on the operator's DXCC entity
    (UN DX uses sameDxccEntity / sameContinent vs partner)."""
    station = {
        "callsign": operator_call,
        "operator": "Steve",
    }
    location = {}
    if state:    location["state"] = state
    if grid:     location["grid"] = grid
    if cq_zone:  location["cq_zone"] = cq_zone
    if itu_zone: location["itu_zone"] = itu_zone
    if location:
        station["location"] = location
    return {
        "contest": {
            "categories": {
                "userPrompt_myExchange": my_exchange,
                "userPrompt_operatingClass": op_class,
                "userPrompt_stationType": station_type,
            },
            "contest_file": "undx.json",
            "mode": "",
            "name": "UN DX Contest",
            "type": "UN DX Contest",
            "version": "1.0.0",
            "year": 2026
        },
        "created": "2026-05-16T06:00:00",
        "exchange_fields": [],
        "format": "ContestLogX",
        "format_version": "1.0",
        "station": station,
        "modified": "",
        "qsos": [],
        "statistics": {"score": 0, "total_points": 0, "total_qsos": 0},
    }


def gen_un_log(rng, kda_codes, target_total=115, dupe_count=3):
    """UN8ABC from G01 (Almaty Almalinskiy). Kazakhstan station — works
    everyone:
      ~15% other UN stations (KDA codes received — own-country 10pt QSOs),
      ~30% AS DX (same continent — 3pt QSOs, e.g., JA/HL/RA/UR partners),
      ~55% non-AS DX (different continent — 5pt QSOs, e.g., G/DL/W partners).
    """
    log = build_skeleton("UN8ABC", "UN", "G01", "SO_AB_MIX",
                          state="Almaty", cq_zone=17, itu_zone=30)
    qsos = []
    next_id = 1
    last_ts = datetime(2026, 5, 16, 6, 0, 0)
    used_keys = set()
    needed_real = target_total - dupe_count
    operator_exch_sent = "G01"
    own_kda = "G01"
    sent_serial = 0  # not used for KDA-code senders

    # AS-continent DX prefixes (separate from the curated DX_PREFIXES set,
    # which is mostly EU/NA/SA). For Kazakhstan, Asia partners are common.
    AS_PREFIXES = ["JA", "HL"]

    for _ in range(needed_real):
        last_ts = last_ts + timedelta(seconds=rng.randint(180, 700))
        mode = pick_mode(rng)
        band = pick_band_for_mode(rng, mode)
        log_mode = cw_or_ssb_log_mode(mode, band)
        freq = BAND_FREQ_PRESETS[band][mode]

        cat_roll = rng.random()
        if cat_roll < 0.15:
            # Other UN station — KDA code.
            their_kda = rng.choice(kda_codes)
            while their_kda == own_kda:
                their_kda = rng.choice(kda_codes)
            exch_r = their_kda
            call = gen_un_call(rng)
        elif cat_roll < 0.45:
            # AS-continent DX station — sends serial.
            sent_serial += 1
            exch_r = f"{rng.randint(1, 200):03d}"
            prefix = rng.choice(AS_PREFIXES)
            digit = str(rng.randint(0, 9))
            suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=rng.choice([2, 3])))
            call = f"{prefix}{digit}{suffix}"
        else:
            # Different-continent DX station — sends serial.
            sent_serial += 1
            exch_r = f"{rng.randint(1, 200):03d}"
            call = gen_dx_call(rng)

        # Avoid same-band/mode duplicates.
        for _ in range(20):
            if (call, band, mode) not in used_keys:
                break
            if exch_r.isdigit():
                # Re-roll based on which category we picked — easier to just
                # re-roll a generic DX call.
                call = gen_dx_call(rng)
            else:
                call = gen_un_call(rng)
        used_keys.add((call, band, mode))

        rst_set = rst_for_mode(mode)
        qsos.append(make_qso(next_id, last_ts, call, band, log_mode, freq,
                              exch_r, operator_exch_sent, rst_set))
        next_id += 1

    qsos += insert_dupes(qsos, rng, dupe_count, next_id)
    log["qsos"] = qsos
    return log


def gen_dx_log(rng, kda_codes, target_total=115, dupe_count=3):
    """W4ABC from FL. DX station — works everyone:
      ~25% UN stations (KDA codes received — high-value 10pt QSOs),
      ~25% other US (same DXCC — 2pt QSOs),
      ~10% NA non-US (same continent — 3pt QSOs),
      ~40% other DX (different continent — 5pt QSOs).
    """
    log = build_skeleton("W4ABC", "DX", "001", "SO_AB_MIX",
                          state="FL", cq_zone=5, itu_zone=8)
    qsos = []
    next_id = 1
    last_ts = datetime(2026, 5, 16, 6, 0, 0)
    used_keys = set()
    needed_real = target_total - dupe_count
    sent_serial = 0
    # NA-continent DX (Canada, Mexico, etc.) — for "same continent, different DXCC"
    NA_DX_PREFIXES = ["XE"]  # Mexico
    VE_PREFIXES = ["VE3", "VE7"]

    for _ in range(needed_real):
        last_ts = last_ts + timedelta(seconds=rng.randint(180, 700))
        mode = pick_mode(rng)
        band = pick_band_for_mode(rng, mode)
        log_mode = cw_or_ssb_log_mode(mode, band)
        freq = BAND_FREQ_PRESETS[band][mode]
        sent_serial += 1
        my_serial_str = f"{sent_serial:03d}"

        cat_roll = rng.random()
        if cat_roll < 0.25:
            # UN station — sends KDA code.
            their_kda = rng.choice(kda_codes)
            exch_r = their_kda
            call = gen_un_call(rng)
        elif cat_roll < 0.50:
            # Another US station — sends serial.
            exch_r = f"{rng.randint(1, 200):03d}"
            call = gen_us_call(rng)
        elif cat_roll < 0.60:
            # NA non-US (Canada/Mexico) — sends serial.
            exch_r = f"{rng.randint(1, 200):03d}"
            if rng.random() < 0.5:
                pfx = rng.choice(VE_PREFIXES)
                suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=rng.choice([2, 3])))
                call = f"{pfx}{suffix}"
            else:
                pfx = rng.choice(NA_DX_PREFIXES)
                digit = str(rng.randint(0, 9))
                suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=rng.choice([2, 3])))
                call = f"{pfx}{digit}{suffix}"
        else:
            # Other DX (different continent) — sends serial.
            exch_r = f"{rng.randint(1, 200):03d}"
            call = gen_dx_call(rng)

        for _ in range(20):
            if (call, band, mode) not in used_keys:
                break
            if exch_r.isdigit():
                call = gen_dx_call(rng)
            else:
                call = gen_un_call(rng)
        used_keys.add((call, band, mode))

        rst_set = rst_for_mode(mode)
        qsos.append(make_qso(next_id, last_ts, call, band, log_mode, freq,
                              exch_r, my_serial_str, rst_set))
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

    kda_codes = load_undx_data()

    for path, gen_fn, label in [
        (UN_LOG, gen_un_log, "UN op (Almaty)"),
        (DX_LOG, gen_dx_log, "DX op (USA)"),
    ]:
        rng = random.Random(args.seed)
        actual_dupes = rng.choice([1, 2, 3, 4])
        rng = random.Random(args.seed)
        log = gen_fn(rng, kda_codes,
                     target_total=args.total, dupe_count=actual_dupes)
        log["statistics"]["total_qsos"] = len(log["qsos"])
        log["modified"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S")
        with open(path, "w") as f:
            json.dump(log, f, indent=4)
            f.write("\n")
        print(f"{label}: {len(log['qsos'])} QSOs ({actual_dupes} dupes) -> {path}")


if __name__ == "__main__":
    main()
