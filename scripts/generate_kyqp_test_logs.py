#!/usr/bin/env python3
"""Generate Kentucky QSO Party (KYQP) automated test logs.

Produces two CLX logs exercising the kyqp.json definition:

  test_kyqp_instate.clx     — a KY station (Fayette county) working KY
                              counties + US states + Canadian provinces + DX.
  test_kyqp_outofstate.clx  — a non-KY (MA) station working KY counties only.

Both exercise: multsOnce dedup (re-working a county/state on another band
adds points but no new multiplier), perBandAndMode dupe checking, the
power-level final-score multiplier, and DX-as-points-only.

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

# 120 Kentucky county abbreviations (from the official 2026 KYQP rules).
KY_COUNTIES = [
    "ADA", "ALL", "AND", "BAL", "BAR", "BAT", "BEL", "BOO", "BOU", "BOY",
    "BOL", "BRA", "BRE", "BRK", "BUL", "BUT", "CAL", "CAW", "CAM", "CAE",
    "CRL", "CTR", "CAS", "CHR", "CLA", "CLY", "CLI", "CRI", "CUM", "DAV",
    "EDM", "ELL", "EST", "FAY", "FLE", "FLO", "FRA", "FUL", "GAL", "GAR",
    "GRT", "GRV", "GRY", "GRE", "GRP", "HAN", "HAR", "HRL", "HSN", "HRT",
    "HEN", "HNY", "HIC", "HOP", "JAC", "JEF", "JES", "JOH", "KEN", "KNT",
    "KNX", "LAR", "LAU", "LAW", "LEE", "LES", "LET", "LEW", "LIN", "LIV",
    "LOG", "LYO", "MCC", "MCY", "MCL", "MAD", "MAG", "MAR", "MSL", "MAT",
    "MAS", "MEA", "MEN", "MER", "MET", "MON", "MOT", "MOR", "MUH", "NEL",
    "NIC", "OHI", "OLD", "OWE", "OWS", "PEN", "PER", "PIK", "POW", "PUL",
    "ROB", "ROC", "ROW", "RUS", "SCO", "SHE", "SIM", "SPE", "TAY", "TOD",
    "TRI", "TRM", "UNI", "WAR", "WAS", "WAY", "WEB", "WHI", "WOL", "WOO",
]
US_STATES = ["AL", "AZ", "CA", "CO", "FL", "GA", "IL", "MA",
             "NY", "OH", "TX", "VA", "WA", "WI", "TN"]
PROVINCES = ["ON", "QC", "BC", "AB", "MB", "NS", "NB", "SK"]

# (band, mode) -> (frequency kHz, mode string stored in the QSO record)
SLOT = {
    ("20m", "CW"):  (14040, "CW"),
    ("40m", "CW"):  (7040, "CW"),
    ("15m", "CW"):  (21040, "CW"),
    ("40m", "PH"):  (7190, "LSB"),
    ("80m", "PH"):  (3810, "LSB"),
}

START = datetime(2026, 6, 6, 13, 0, 0)  # 1300Z Saturday June 6 2026


def make_pool(seed, us, canada, intl):
    """Return disjoint lists of unique US / Canadian / international calls."""
    random.seed(seed)
    seen, out = set(), {"US": [], "CA": [], "DX": []}
    need = {"US": us, "CA": canada, "DX": intl}
    fn = {"US": gc.generate_us_callsign,
          "CA": gc.generate_canadian_callsign,
          "DX": gc.generate_international_callsign}
    for kind in ("US", "CA", "DX"):
        while len(out[kind]) < need[kind]:
            c = fn[kind]()
            if c not in seen:
                seen.add(c)
                out[kind].append(c)
    return out


def qso(idx, ts, call, band, mode, rst, exch_s, exch_r):
    freq, mode_str = SLOT[(band, mode)]
    return {
        "band": band,
        "callsign": call,
        "duplicate": False,
        "dxcc_count": 0,
        "exchange_fields": {
            "CALL": call,
            "EXCHr": exch_r,
            "EXCHs": exch_s,
            "RSTr": rst,
            "RSTs": rst,
            "SNs": str(idx),
        },
        "frequency": freq,
        "grid_square_count": 0,
        "id": idx,
        "mode": mode_str,
        "multiplier_count": 0,
        "points": 0,
        "rst_received": rst,
        "rst_sent": rst,
        "serial": idx,
        "timestamp": ts.strftime("%Y-%m-%dT%H:%M:%SZ"),
    }


def write_log(path, station, location, categories, qsos):
    doc = {
        "contest": {
            "categories": categories,
            "contest_file": "kyqp.json",
            "mode": "",
            "name": "Kentucky QSO Party",
            "type": "Kentucky QSO Party",
            "version": "1.0.0",
            "year": 2026,
        },
        "created": "2026-06-06T13:00:00",
        "exchange_fields": [],
        "format": "ContestLogX",
        "format_version": "1.0",
        "modified": "2026-06-06T01:30:00",
        "qsos": qsos,
        "statistics": {"score": 0, "total_points": 0, "total_qsos": len(qsos)},
        "station": {"callsign": station, "operator": "Steve", "location": location},
    }
    path.write_text(json.dumps(doc, indent=4) + "\n")
    print(f"wrote {path.name}: {len(qsos)} QSOs")


def build_instate():
    """KY station in Fayette county. powerCategory LP -> final score x2.

    Phase A  20 KY counties        20m CW   20 x 2 = 40 pts, 20 county mults
    Phase B  15 US states          40m CW   15 x 2 = 30 pts, 15 state mults
    Phase C   8 CA provinces       40m SSB   8 x 1 =  8 pts,  8 prov  mults
    Phase D   5 DX                 15m CW    5 x 2 = 10 pts,  0 mults
    Phase E   3 dupes (Phase A)    20m CW    0 pts,           0 mults
    Phase F   4 KY (counties from  80m SSB   4 x 1 =  4 pts,  0 new mults
             Phase A, new calls)                              (multsOnce)
    Total: 55 QSOs, 92 points, 43 mults -> 92 * 43 * 2 = 7912
    """
    p = make_pool(seed=606, us=39, canada=8, intl=5)
    us, ca, dx = p["US"], p["CA"], p["DX"]
    qsos, t, idx = [], START, 1

    def add(call, band, mode, rst, exch_r):
        nonlocal t, idx
        qsos.append(qso(idx, t, call, band, mode, rst, "FAY", exch_r))
        t += timedelta(minutes=2)
        idx += 1

    phaseA_calls = us[:20]
    for call, county in zip(phaseA_calls, KY_COUNTIES[:20]):       # A
        add(call, "20m", "CW", "599", county)
    for call, st in zip(us[20:35], US_STATES):                     # B
        add(call, "40m", "CW", "599", st)
    for call, prov in zip(ca, PROVINCES):                          # C
        add(call, "40m", "PH", "59", prov)
    for call in dx:                                                # D
        add(call, "15m", "CW", "599", "DX")
    for call, county in zip(phaseA_calls[:3], KY_COUNTIES[:3]):    # E (dupes)
        add(call, "20m", "CW", "599", county)
    for call, county in zip(us[35:39], KY_COUNTIES[:4]):           # F
        add(call, "80m", "PH", "59", county)

    write_log(
        TEST_LOGS / "test_kyqp_instate.clx",
        station=gc.generate_us_callsign(),
        location={"state": "KY", "cq_zone": 4, "itu_zone": 8},
        categories={
            "userPrompt_stationType": "KY",
            "userPrompt_operatingClass": "KY_FIXED_SO_MIXED",
            "userPrompt_powerCategory": "LP",
            "userPrompt_myExchange": "FAY",
        },
        qsos=qsos,
    )
    return 92 * 43 * 2


def build_outofstate():
    """Non-KY station in Massachusetts. powerCategory HP -> final score x1.

    Phase A  25 KY counties        20m CW   25 x 2 = 50 pts, 25 county mults
    Phase B  12 KY counties (new)  40m SSB  12 x 1 = 12 pts, 12 county mults
    Phase C   6 KY (counties from  15m CW    6 x 2 = 12 pts,  0 new mults
             Phase A, new calls)                              (multsOnce)
    Phase D   3 dupes (Phase A)    20m CW    0 pts,           0 mults
    Total: 46 QSOs, 74 points, 37 mults -> 74 * 37 * 1 = 2738
    """
    p = make_pool(seed=607, us=43, canada=0, intl=0)
    us = p["US"]
    qsos, t, idx = [], START, 1

    def add(call, band, mode, rst, county):
        nonlocal t, idx
        qsos.append(qso(idx, t, call, band, mode, rst, "MA", county))
        t += timedelta(minutes=2)
        idx += 1

    phaseA = us[:25]
    for call, county in zip(phaseA, KY_COUNTIES[:25]):             # A
        add(call, "20m", "CW", "599", county)
    for call, county in zip(us[25:37], KY_COUNTIES[25:37]):        # B
        add(call, "40m", "PH", "59", county)
    for call, county in zip(us[37:43], KY_COUNTIES[:6]):           # C
        add(call, "15m", "CW", "599", county)
    for call, county in zip(phaseA[:3], KY_COUNTIES[:3]):          # D (dupes)
        add(call, "20m", "CW", "599", county)

    write_log(
        TEST_LOGS / "test_kyqp_outofstate.clx",
        station=gc.generate_us_callsign(),
        location={"state": "MA", "cq_zone": 5, "itu_zone": 8},
        categories={
            "userPrompt_stationType": "OUTSIDE",
            "userPrompt_operatingClass": "NON_KY_SO",
            "userPrompt_powerCategory": "HP",
            "userPrompt_myExchange": "MA",
        },
        qsos=qsos,
    )
    return 74 * 37 * 1


if __name__ == "__main__":
    s_in = build_instate()
    s_out = build_outofstate()
    print(f"expected in-state score:     {s_in}")
    print(f"expected out-of-state score: {s_out}")
