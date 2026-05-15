#!/usr/bin/env python3
"""Generate West Virginia QSO Party (WVQP) automated test logs.

Produces two CLX logs exercising the wvqp.json definition:

  test_wvqp_instate.clx     — a WV station (Kanawha county) working WV
                              counties + US states + Canadian provinces +
                              DXCC entities, across CW / SSB / Digital.
  test_wvqp_outofstate.clx  — a non-WV (OH) station working WV counties only.

Both exercise: multsOnce dedup, perBandAndMode dupe checking with Digital
as its own mode, the W8WVA per-band/mode bonus station, and (in-state)
DXCC multipliers for WV operators.

Callsigns come from scripts/generate_callsigns.py (per project convention —
do not hand-write callsigns for test data).
"""

import json
import re
import sys
from datetime import datetime, timedelta
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import random
import generate_callsigns as gc

REPO = Path(__file__).resolve().parent.parent
TEST_LOGS = REPO / "test_logs"

# 55 West Virginia county abbreviations (from the official WVQP rules).
WV_COUNTIES = [
    "BAR", "BER", "BOO", "BRA", "BRO", "CAB", "CAL", "CLA", "DOD", "FAY",
    "GIL", "GRA", "GRE", "HAM", "HAN", "HDY", "HAR", "JAC", "JEF", "KAN",
    "LEW", "LIN", "LOG", "MRN", "MAR", "MAS", "MCD", "MER", "MIN", "MGO",
    "MON", "MRO", "MOR", "NIC", "OHI", "PEN", "PLE", "POC", "PRE", "PUT",
    "RAL", "RAN", "RIT", "ROA", "SUM", "TAY", "TUC", "TYL", "UPS", "WAY",
    "WEB", "WET", "WIR", "WOO", "WYO",
]
US_STATES = ["AL", "AZ", "CA", "CO", "FL", "GA", "IL", "IN", "MA", "NY",
             "OH", "TX", "VA", "WI", "TN", "SC"]
PROVINCES = ["ON", "QC", "BC", "AB", "MB", "NS"]

# International prefix -> DXCC entity, used to pick DX calls with distinct
# entities so the DXCC multiplier count is deterministic.
PREFIX_ENTITY = {
    "G": "England", "M": "England", "GW": "Wales", "GI": "NIreland",
    "DL": "Germany", "DJ": "Germany", "DK": "Germany", "F": "France",
    "I": "Italy", "EA": "Spain", "EB": "Spain", "OH": "Finland",
    "SM": "Sweden", "LA": "Norway", "OZ": "Denmark", "PA": "Netherlands",
    "PE": "Netherlands", "ON": "Belgium", "HB": "Switzerland", "OE": "Austria",
    "HA": "Hungary", "OK": "Czech", "JA": "Japan", "JE": "Japan",
    "JH": "Japan", "JR": "Japan", "HL": "Korea", "BV": "Taiwan",
    "VK": "Australia", "ZL": "NewZealand", "PY": "Brazil", "LU": "Argentina",
    "CE": "Chile", "YV": "Venezuela", "PJ": "Curacao", "ZS": "SouthAfrica",
}

# (band, slot) -> (frequency kHz, mode string stored in the QSO record)
SLOT = {
    ("20m", "CW"):  (14045, "CW"),
    ("40m", "CW"):  (7045, "CW"),
    ("15m", "CW"):  (21045, "CW"),
    ("80m", "CW"):  (3545, "CW"),
    ("40m", "PH"):  (7185, "LSB"),
    ("20m", "DIG"): (14080, "RTTY"),
}

START = datetime(2026, 6, 20, 16, 0, 0)  # 1600Z Saturday June 20 2026


def make_us(seed, n):
    random.seed(seed)
    seen, out = set(), []
    while len(out) < n:
        c = gc.generate_us_callsign()
        if c not in seen:
            seen.add(c)
            out.append(c)
    return out


def make_canada(seed, n):
    random.seed(seed)
    seen, out = set(), []
    while len(out) < n:
        c = gc.generate_canadian_callsign()
        if c not in seen:
            seen.add(c)
            out.append(c)
    return out


def make_dx(seed, n):
    """Return n international calls, each from a distinct DXCC entity."""
    random.seed(seed)
    seen_calls, entities, out = set(), set(), []
    while len(out) < n:
        c = gc.generate_international_callsign()
        m = re.match(r"^([A-Z]+)", c)
        if not m or c in seen_calls:
            continue
        entity = PREFIX_ENTITY.get(m.group(1))
        if entity and entity not in entities:
            entities.add(entity)
            seen_calls.add(c)
            out.append(c)
    return out


def qso(idx, ts, call, band, slot, rst, exch_s, exch_r):
    freq, mode_str = SLOT[(band, slot)]
    return {
        "band": band,
        "callsign": call,
        "duplicate": False,
        "dxcc_count": 0,
        "exchange_fields": {
            "CALL": call, "EXCHr": exch_r, "EXCHs": exch_s,
            "RSTr": rst, "RSTs": rst, "SNs": str(idx),
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
            "contest_file": "wvqp.json",
            "mode": "",
            "name": "West Virginia QSO Party",
            "type": "West Virginia QSO Party",
            "version": "1.0.0",
            "year": 2026,
        },
        "created": "2026-06-20T16:00:00",
        "exchange_fields": [],
        "format": "ContestLogX",
        "format_version": "1.0",
        "modified": "2026-06-21T04:00:00",
        "qsos": qsos,
        "statistics": {"score": 0, "total_points": 0, "total_qsos": len(qsos)},
        "station": {"callsign": station, "operator": "Steve", "location": location},
    }
    path.write_text(json.dumps(doc, indent=4) + "\n")
    print(f"wrote {path.name}: {len(qsos)} QSOs")


def build_instate():
    """WV station in Kanawha county.

    A  20 WV counties      20m CW    20 x 2 = 40 pts, 20 county mults
    B  12 US states        40m CW    12 x 2 = 24 pts, 12 state  mults
    C   6 CA provinces     40m SSB    6 x 1 =  6 pts,  6 prov   mults
    D   5 DX               15m CW     5 x 2 = 10 pts,  5 DXCC   mults
    E   4 US states (new)  20m DIG    4 x 2 =  8 pts,  4 state  mults
    F   1 W8WVA QSO        80m CW     1 x 2 =  2 pts,  0 new mult, +100 bonus
    G   3 dupes (Phase A)  20m CW     0 pts,           0 mults
    Total: 51 QSOs, 90 points, 47 mults, +100 bonus -> 90 * 47 + 100 = 4330
    """
    us = make_us(620, 39)
    ca = make_canada(621, 6)
    dx = make_dx(622, 5)
    qsos, t, idx = [], START, 1

    def add(call, band, slot, rst, exch_r):
        nonlocal t, idx
        qsos.append(qso(idx, t, call, band, slot, rst, "KAN", exch_r))
        t += timedelta(minutes=2)
        idx += 1

    a_calls = us[:20]
    for call, county in zip(a_calls, WV_COUNTIES[:20]):           # A
        add(call, "20m", "CW", "599", county)
    for call, st in zip(us[20:32], US_STATES[:12]):               # B
        add(call, "40m", "CW", "599", st)
    for call, prov in zip(ca, PROVINCES):                         # C
        add(call, "40m", "PH", "59", prov)
    for call in dx:                                               # D
        add(call, "15m", "CW", "599", "DX")
    for call, st in zip(us[32:36], US_STATES[12:16]):             # E
        add(call, "20m", "DIG", "599", st)
    add("W8WVA", "80m", "CW", "599", WV_COUNTIES[0])              # F
    for call, county in zip(a_calls[:3], WV_COUNTIES[:3]):        # G (dupes)
        add(call, "20m", "CW", "599", county)

    write_log(
        TEST_LOGS / "test_wvqp_instate.clx",
        station=us[36],
        location={"state": "WV", "cq_zone": 5, "itu_zone": 8},
        categories={
            "userPrompt_stationType": "WV",
            "userPrompt_operatingClass": "SINGLE_OP",
            "userPrompt_powerCategory": "HP",
            "userPrompt_myExchange": "KAN",
        },
        qsos=qsos,
    )
    return 90 * 47 + 100


def build_outofstate():
    """Non-WV station in Ohio.

    A  25 WV counties       20m CW   25 x 2 = 50 pts, 25 county mults
    B  12 WV counties (new) 40m SSB  12 x 1 = 12 pts, 12 county mults
    C   6 WV counties (new) 20m DIG   6 x 2 = 12 pts,  6 county mults
    D   6 WV (counties from 15m CW    6 x 2 = 12 pts,  0 new mults
       Phase A, new calls)                            (multsOnce)
    E   1 W8WVA QSO         80m CW    1 x 2 =  2 pts,  0 new mult, +100 bonus
    F   3 dupes (Phase A)   20m CW    0 pts,           0 mults
    Total: 53 QSOs, 88 points, 43 mults, +100 bonus -> 88 * 43 + 100 = 3884
    """
    us = make_us(623, 50)
    qsos, t, idx = [], START, 1

    def add(call, band, slot, rst, county):
        nonlocal t, idx
        qsos.append(qso(idx, t, call, band, slot, rst, "OH", county))
        t += timedelta(minutes=2)
        idx += 1

    a_calls = us[:25]
    for call, county in zip(a_calls, WV_COUNTIES[:25]):           # A
        add(call, "20m", "CW", "599", county)
    for call, county in zip(us[25:37], WV_COUNTIES[25:37]):       # B
        add(call, "40m", "PH", "59", county)
    for call, county in zip(us[37:43], WV_COUNTIES[37:43]):       # C
        add(call, "20m", "DIG", "599", county)
    for call, county in zip(us[43:49], WV_COUNTIES[:6]):          # D
        add(call, "15m", "CW", "599", county)
    add("W8WVA", "80m", "CW", "599", WV_COUNTIES[0])              # E
    for call, county in zip(a_calls[:3], WV_COUNTIES[:3]):        # F (dupes)
        add(call, "20m", "CW", "599", county)

    write_log(
        TEST_LOGS / "test_wvqp_outofstate.clx",
        station=us[49],
        location={"state": "OH", "cq_zone": 4, "itu_zone": 8},
        categories={
            "userPrompt_stationType": "OUTSIDE",
            "userPrompt_operatingClass": "SINGLE_OP",
            "userPrompt_powerCategory": "LP",
            "userPrompt_myExchange": "OH",
        },
        qsos=qsos,
    )
    return 88 * 43 + 100


if __name__ == "__main__":
    s_in = build_instate()
    s_out = build_outofstate()
    print(f"expected in-state score:     {s_in}")
    print(f"expected out-of-state score: {s_out}")
