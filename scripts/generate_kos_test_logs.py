#!/usr/bin/env python3
"""
Generate King of Spain CW test logs from scratch.

Reads:  contests/kos.json (province list)
Writes: test_logs/test_kos_ea.clx     (Spanish op from EA1, Madrid province)
        test_logs/test_kos_dx.clx     (USA op from FL)

Usage:  python3 scripts/generate_kos_test_logs.py [--seed 23]
"""

import argparse
import json
import os
import random
from datetime import datetime, timedelta, timezone

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTEST_FILE = os.path.join(REPO_ROOT, "contests", "kos.json")
EA_LOG = os.path.join(REPO_ROOT, "test_logs", "test_kos_ea.clx")
DX_LOG = os.path.join(REPO_ROOT, "test_logs", "test_kos_dx.clx")

# 6 HF bands — CW only.
BAND_FREQ_PRESETS = {
    "160m": 1825,
    "80m":  3535,
    "40m":  7035,
    "20m":  14035,
    "15m":  21035,
    "10m":  28035,
}

BAND_WEIGHTS = {"160m": 4, "80m": 18, "40m": 32, "20m": 30, "15m": 12, "10m": 4}

# Spanish provinces by call area (from contests/kos.json).
PROVINCES_BY_AREA = {
    "EA1": ["AV", "BU", "C", "LE", "LO", "LU", "O", "OU", "P", "PO", "S", "SA", "SG", "SO", "VA", "ZA"],
    "EA2": ["BI", "HU", "NA", "SS", "TE", "VI", "Z"],
    "EA3": ["B", "GI", "L", "T"],
    "EA4": ["BA", "CC", "CR", "CU", "GU", "M", "TO"],
    "EA5": ["A", "AB", "CS", "MU", "V"],
    "EA6": ["IB"],
    "EA7": ["AL", "CA", "CO", "GR", "H", "J", "MA", "SE"],
    "EA8": ["GC", "TF"],
    "EA9": ["CE", "ML"],
}

# DX prefixes — same curated 1:1 prefix→EADX-100 list used for predictable
# entity counts. Each maps cleanly to a single EADX-100 entity for any digit.
DX_PREFIXES = ["G", "DL", "F", "I", "JA", "PA", "OE", "SM", "OZ", "OK",
               "SP", "LA", "EI", "LU", "XE"]

US_PREFIXES = ["W", "K", "N"]


def gen_ea_call(rng, area):
    """Generate a Spanish callsign for the given EA area."""
    prefix = area  # e.g., "EA1", "EA8", etc.
    suffix_len = rng.choices([2, 3], weights=[60, 40])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{suffix}"


def gen_dx_call(rng):
    """Generate a non-Spanish DX call from the curated safe-prefix list."""
    prefix = rng.choice(DX_PREFIXES)
    digit = str(rng.randint(0, 9))
    suffix_len = rng.choices([2, 3], weights=[55, 45])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{digit}{suffix}"


def gen_us_call(rng):
    """Generate a USA callsign."""
    prefix = rng.choice(US_PREFIXES)
    digit = str(rng.randint(0, 9))
    suffix_len = rng.choices([2, 3], weights=[55, 45])[0]
    suffix = "".join(rng.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ", k=suffix_len))
    return f"{prefix}{digit}{suffix}"


def load_kos_data():
    with open(CONTEST_FILE, "r") as f:
        contest = json.load(f)
    return contest["validation"]["namedMults"]


def pick_band(rng):
    bands = list(BAND_WEIGHTS.keys())
    return rng.choices(bands, weights=[BAND_WEIGHTS[b] for b in bands])[0]


def make_qso(qso_id, ts, callsign, band, freq, exch_recvd, exch_sent, duplicate=False):
    return {
        "band": band,
        "callsign": callsign,
        "duplicate": duplicate,
        "dxcc_count": 0,
        "exchange_fields": {
            "CALL":  callsign,
            "EXCHr": exch_recvd,
            "EXCHs": exch_sent,
            "RSTr":  "599",
            "RSTs":  "599",
            "SNs":   str(qso_id),
        },
        "frequency": freq,
        "grid_square_count": 0,
        "id": qso_id,
        "mode": "CW",
        "multiplier_count": 0,
        "points": 0,
        "rst_received": "599",
        "rst_sent": "599",
        "serial": qso_id,
        "timestamp": ts.strftime("%Y-%m-%dT%H:%M:%SZ"),
    }


def build_skeleton(operator_call, station_type, my_exchange, op_class, qth):
    return {
        "contest": {
            "categories": {
                "userPrompt_myExchange": my_exchange,
                "userPrompt_operatingClass": op_class,
                "userPrompt_stationType": station_type,
            },
            "contest_file": "kos.json",
            "mode": "",
            "name": "His Majesty The King of Spain CW",
            "type": "His Majesty The King of Spain CW",
            "version": "1.0.0",
            "year": 2026
        },
        "created": "2026-05-16T12:00:00",
        "exchange_fields": [],
        "format": "ContestLogX",
        "format_version": "1.0",
        "metadata": {
            "callsign": operator_call,
            "club": "",
            "name": "Steve",
            "qth": qth,
        },
        "modified": "",
        "qsos": [],
        "statistics": {"score": 0, "total_points": 0, "total_qsos": 0},
    }


def gen_ea_log(rng, target_total=115, dupe_count=3):
    """EA1ABC from M (Madrid). Spanish station — works everyone:
      ~30% other EA stations (province codes received),
      ~70% DX stations (serial numbers received).
    """
    log = build_skeleton("EA1ABC", "EA", "M", "SO_HP", "Madrid, Spain")
    qsos = []
    next_id = 1
    last_ts = datetime(2026, 5, 16, 12, 0, 0)
    used_keys = set()
    needed_real = target_total - dupe_count
    sent_serial = 1  # not used for province senders, but kept for code shape

    operator_call = "EA1ABC"
    operator_exch_sent = "M"  # Madrid

    for _ in range(needed_real):
        # 24-hour window, ~115 QSOs => ~12 minutes apart on average.
        last_ts = last_ts + timedelta(seconds=rng.randint(300, 1000))
        band = pick_band(rng)
        freq = BAND_FREQ_PRESETS[band]

        if rng.random() < 0.30:
            # Other Spanish station — pick any EA area, then any province.
            area = rng.choice(list(PROVINCES_BY_AREA.keys()))
            their_province = rng.choice(PROVINCES_BY_AREA[area])
            # Avoid sending own province (M) back as received.
            while their_province == operator_exch_sent:
                area = rng.choice(list(PROVINCES_BY_AREA.keys()))
                their_province = rng.choice(PROVINCES_BY_AREA[area])
            exch_r = their_province
            call = gen_ea_call(rng, area)
        else:
            # DX station — sends a serial number.
            sent_serial += 1
            exch_r = f"{sent_serial:03d}"
            call = gen_dx_call(rng)

        # Avoid same-band duplicates.
        for _ in range(20):
            if (call, band) not in used_keys:
                break
            if exch_r.isdigit():
                call = gen_dx_call(rng)
            else:
                # Find which area the province belongs to.
                for a, plist in PROVINCES_BY_AREA.items():
                    if exch_r in plist:
                        call = gen_ea_call(rng, a)
                        break
        used_keys.add((call, band))

        qsos.append(make_qso(next_id, last_ts, call, band, freq,
                              exch_r, operator_exch_sent))
        next_id += 1

    # Insert dupes
    candidates = [q for q in qsos if not q.get("duplicate", False)]
    for _ in range(dupe_count):
        original = rng.choice(candidates)
        orig_ts = datetime.fromisoformat(original["timestamp"].replace("Z", "+00:00"))
        dupe_ts = orig_ts + timedelta(seconds=rng.randint(60, 240))
        dupe = make_qso(
            qso_id=next_id, ts=dupe_ts,
            callsign=original["callsign"],
            band=original["band"],
            freq=original["frequency"],
            exch_recvd=original["exchange_fields"]["EXCHr"],
            exch_sent=original["exchange_fields"]["EXCHs"],
            duplicate=True,
        )
        qsos.append(dupe)
        next_id += 1

    log["qsos"] = qsos
    return log


def gen_dx_log(rng, target_total=115, dupe_count=3):
    """W4ABC from FL. DX station — works everyone:
      ~50% Spanish stations (province codes received — high-value 3-pt QSOs),
      ~50% other DX stations (serial numbers received — 1-pt QSOs).
    """
    log = build_skeleton("W4ABC", "DX", "001", "SO_HP", "FL")
    qsos = []
    next_id = 1
    last_ts = datetime(2026, 5, 16, 12, 0, 0)
    used_keys = set()
    needed_real = target_total - dupe_count
    sent_serial = 0
    operator_exch_sent = ""  # placeholder, will be filled with the per-QSO serial

    for _ in range(needed_real):
        last_ts = last_ts + timedelta(seconds=rng.randint(300, 1000))
        band = pick_band(rng)
        freq = BAND_FREQ_PRESETS[band]
        sent_serial += 1
        my_serial_str = f"{sent_serial:03d}"

        if rng.random() < 0.50:
            # Spanish station — sends province code.
            area = rng.choice(list(PROVINCES_BY_AREA.keys()))
            their_province = rng.choice(PROVINCES_BY_AREA[area])
            exch_r = their_province
            call = gen_ea_call(rng, area)
        else:
            # Another DX station — sends a serial number.
            exch_r = f"{rng.randint(1, 200):03d}"
            call = gen_dx_call(rng)

        for _ in range(20):
            if (call, band) not in used_keys:
                break
            if exch_r.isdigit():
                call = gen_dx_call(rng)
            else:
                for a, plist in PROVINCES_BY_AREA.items():
                    if exch_r in plist:
                        call = gen_ea_call(rng, a)
                        break
        used_keys.add((call, band))

        qsos.append(make_qso(next_id, last_ts, call, band, freq,
                              exch_r, my_serial_str))
        next_id += 1

    # Insert dupes
    candidates = [q for q in qsos if not q.get("duplicate", False)]
    for _ in range(dupe_count):
        original = rng.choice(candidates)
        orig_ts = datetime.fromisoformat(original["timestamp"].replace("Z", "+00:00"))
        dupe_ts = orig_ts + timedelta(seconds=rng.randint(60, 240))
        dupe = make_qso(
            qso_id=next_id, ts=dupe_ts,
            callsign=original["callsign"],
            band=original["band"],
            freq=original["frequency"],
            exch_recvd=original["exchange_fields"]["EXCHr"],
            exch_sent=original["exchange_fields"]["EXCHs"],
            duplicate=True,
        )
        qsos.append(dupe)
        next_id += 1

    log["qsos"] = qsos
    return log


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=23,
                        help="RNG seed for reproducibility (default 23)")
    parser.add_argument("--total", type=int, default=115,
                        help="Target total QSOs per file (default 115)")
    args = parser.parse_args()

    for path, gen_fn, label in [
        (EA_LOG, gen_ea_log, "EA op (Madrid)"),
        (DX_LOG, gen_dx_log, "DX op (USA)"),
    ]:
        rng = random.Random(args.seed)
        actual_dupes = rng.choice([1, 2, 3, 4])
        rng = random.Random(args.seed)
        log = gen_fn(rng, target_total=args.total, dupe_count=actual_dupes)
        log["statistics"]["total_qsos"] = len(log["qsos"])
        log["modified"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S")
        with open(path, "w") as f:
            json.dump(log, f, indent=4)
            f.write("\n")
        print(f"{label}: {len(log['qsos'])} QSOs ({actual_dupes} dupes) -> {path}")


if __name__ == "__main__":
    main()
