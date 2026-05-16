#!/usr/bin/env python3
"""Generate ARRL Field Day (FD) automated test logs.

Produces two CLX logs exercising the field_day.json definition:

  test_fd_3a.clx   — a 3A club station, LOW power (multiplier x2).
  test_fd_qrp.clx  — a 1B station, QRP on natural power (multiplier x5).

Field Day has no QSO multipliers — score is QSO points x power multiplier.
Both logs exercise the mode-based point values (Phone 1, CW 2, Digital 2),
perBandAndMode dupe checking, and the power-level score multiplier.

Callsigns come from scripts/generate_callsigns.py (per project convention —
do not hand-write callsigns for test data).
"""

import json
import sys
from datetime import datetime, timedelta
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import random
import generate_callsigns as gc

REPO = Path(__file__).resolve().parent.parent
TEST_LOGS = REPO / "test_logs"

# Worked-station exchange values (free-form per the FD definition).
WORKED_CLASSES = ["1A", "2A", "3A", "5A", "1B", "2B", "1C", "1D", "2D",
                  "1E", "2E", "1F", "2F", "3F"]
WORKED_SECTIONS = ["CT", "EMA", "WMA", "ME", "NH", "RI", "VT", "ENY", "NLI",
                   "NNY", "NNJ", "SNJ", "WNY", "DE", "EPA", "MDC", "WPA",
                   "AL", "GA", "KY", "NC", "SC", "TN", "VA", "WV", "OH",
                   "MI", "IL", "IN", "WI", "CO", "MN", "MO", "EB", "LAX",
                   "SCV", "SDG", "AZ", "OR", "WWA", "AB", "BC", "ON", "MB"]

# (band, slot) -> (frequency kHz, mode string stored in the QSO record)
SLOT = {
    ("20m", "PH"):  (14250, "SSB"),
    ("40m", "CW"):  (7030, "CW"),
    ("20m", "DIG"): (14080, "RTTY"),
    ("15m", "DIG"): (21080, "RTTY"),
}

START = datetime(2026, 6, 27, 18, 0, 0)  # 1800Z Saturday June 27 2026


def make_us(seed, n):
    random.seed(seed)
    seen, out = set(), []
    while len(out) < n:
        c = gc.generate_us_callsign()
        if c not in seen:
            seen.add(c)
            out.append(c)
    return out


def qso(idx, ts, call, band, slot, cls_s, sec_s, cls_r, sec_r):
    freq, mode_str = SLOT[(band, slot)]
    return {
        "band": band,
        "callsign": call,
        "duplicate": False,
        "dxcc_count": 0,
        "exchange_fields": {
            "CALL": call,
            "CLASSs": cls_s, "SECTs": sec_s,
            "CLASSr": cls_r, "SECTr": sec_r,
        },
        "frequency": freq,
        "grid_square_count": 0,
        "id": idx,
        "mode": mode_str,
        "multiplier_count": 0,
        "points": 0,
        "rst_received": "",
        "rst_sent": "",
        "serial": idx,
        "timestamp": ts.strftime("%Y-%m-%dT%H:%M:%SZ"),
    }


def write_log(path, station, categories, qsos):
    doc = {
        "contest": {
            "categories": categories,
            "contest_file": "field_day.json",
            "mode": "",
            "name": "ARRL Field Day",
            "type": "ARRL Field Day",
            "version": "1.0.0",
            "year": 2026,
        },
        "created": "2026-06-27T18:00:00",
        "exchange_fields": [],
        "format": "ContestLogX",
        "format_version": "1.0",
        "modified": "2026-06-28T21:00:00",
        "qsos": qsos,
        "statistics": {"score": 0, "total_points": 0, "total_qsos": len(qsos)},
        "station": {"callsign": station, "operator": "Steve"},
    }
    path.write_text(json.dumps(doc, indent=4) + "\n")
    print(f"wrote {path.name}: {len(qsos)} QSOs")


def build_3a():
    """3A club station, LOW power (x2).

    A  20 Phone   20m SSB   20 x 1 = 20 pts
    B  18 CW      40m CW    18 x 2 = 36 pts
    C  12 Digital 20m RTTY  12 x 2 = 24 pts
    D   3 dupes (Phase A)   20m SSB   0 pts
    Total: 53 QSOs, 80 points -> 80 * 2 = 160
    """
    us = make_us(627, 50)
    qsos, t, idx = [], START, 1

    def add(call, band, slot, cls_r, sec_r):
        nonlocal t, idx
        qsos.append(qso(idx, t, call, band, slot, "3A", "CT", cls_r, sec_r))
        t += timedelta(minutes=1)
        idx += 1

    a_calls = us[:20]
    for i, call in enumerate(a_calls):                              # A
        add(call, "20m", "PH", WORKED_CLASSES[i % len(WORKED_CLASSES)],
            WORKED_SECTIONS[i % len(WORKED_SECTIONS)])
    for i, call in enumerate(us[20:38]):                            # B
        add(call, "40m", "CW", WORKED_CLASSES[i % len(WORKED_CLASSES)],
            WORKED_SECTIONS[(i + 5) % len(WORKED_SECTIONS)])
    for i, call in enumerate(us[38:50]):                            # C
        add(call, "20m", "DIG", WORKED_CLASSES[i % len(WORKED_CLASSES)],
            WORKED_SECTIONS[(i + 11) % len(WORKED_SECTIONS)])
    for i, call in enumerate(a_calls[:3]):                          # D (dupes)
        add(call, "20m", "PH", WORKED_CLASSES[i % len(WORKED_CLASSES)],
            WORKED_SECTIONS[i % len(WORKED_SECTIONS)])

    write_log(
        TEST_LOGS / "test_fd_3a.clx",
        station=gc.generate_us_callsign(),
        categories={
            "userPrompt_stationClass": "3A",
            "userPrompt_section": "CT",
            "userPrompt_powerCategory": "LOW",
        },
        qsos=qsos,
    )
    return 80 * 2


def build_qrp():
    """1B station, QRP on natural power (x5).

    A  10 CW      40m CW    10 x 2 = 20 pts
    B   8 Phone   20m SSB    8 x 1 =  8 pts
    C   6 Digital 15m RTTY   6 x 2 = 12 pts
    D   2 dupes (Phase A)    40m CW    0 pts
    Total: 26 QSOs, 40 points -> 40 * 5 = 200
    """
    us = make_us(628, 24)
    qsos, t, idx = [], START, 1

    def add(call, band, slot, cls_r, sec_r):
        nonlocal t, idx
        qsos.append(qso(idx, t, call, band, slot, "1B", "EMA", cls_r, sec_r))
        t += timedelta(minutes=1)
        idx += 1

    a_calls = us[:10]
    for i, call in enumerate(a_calls):                              # A
        add(call, "40m", "CW", WORKED_CLASSES[i % len(WORKED_CLASSES)],
            WORKED_SECTIONS[i % len(WORKED_SECTIONS)])
    for i, call in enumerate(us[10:18]):                            # B
        add(call, "20m", "PH", WORKED_CLASSES[i % len(WORKED_CLASSES)],
            WORKED_SECTIONS[(i + 7) % len(WORKED_SECTIONS)])
    for i, call in enumerate(us[18:24]):                            # C
        add(call, "15m", "DIG", WORKED_CLASSES[i % len(WORKED_CLASSES)],
            WORKED_SECTIONS[(i + 13) % len(WORKED_SECTIONS)])
    for i, call in enumerate(a_calls[:2]):                          # D (dupes)
        add(call, "40m", "CW", WORKED_CLASSES[i % len(WORKED_CLASSES)],
            WORKED_SECTIONS[i % len(WORKED_SECTIONS)])

    write_log(
        TEST_LOGS / "test_fd_qrp.clx",
        station=gc.generate_us_callsign(),
        categories={
            "userPrompt_stationClass": "1B",
            "userPrompt_section": "EMA",
            "userPrompt_powerCategory": "QRP_NATURAL",
        },
        qsos=qsos,
    )
    return 40 * 5


if __name__ == "__main__":
    s_3a = build_3a()
    s_qrp = build_qrp()
    print(f"expected 3A (LOW x2) score:        {s_3a}")
    print(f"expected QRP (natural x5) score:   {s_qrp}")
