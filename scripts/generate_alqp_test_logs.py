#!/usr/bin/env python3
"""Generate Alabama QSO Party (ALQP) automated test logs.

Produces two CLX logs exercising the alqp.json definition:

  test_alqp_instate.clx     - an AL station (Jefferson county) working AL
                              counties + US states + Canadian provinces + DX.
  test_alqp_outofstate.clx  - a non-AL (TN) station working AL counties only.

Both exercise: multsPerMode dedup (same mult on CW vs SSB counts as two
separate mults), perBandAndMode dupe checking, namedMultAliases
(DC and MD both alias to MDC), stationClassMultipliers gating DXCC to
AL ops only.

Callsigns come from scripts/generate_callsigns.py (per project convention -
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

# 67 Alabama counties (per AQP rules).
AL_COUNTIES = [
    "AUTA", "BALD", "BARB", "BIBB", "BLOU", "BULL", "BUTL", "CHOU", "CHMB", "CKEE",
    "CHIL", "CHOC", "CLRK", "CLAY", "CLEB", "COFF", "COLB", "CONE", "COOS", "COVI",
    "CREN", "CULM", "DALE", "DLLS", "DKLB", "ELMO", "ESCA", "ETOW", "FAYE", "FRNK",
    "GENE", "GREE", "HALE", "HNRY", "HOUS", "JKSN", "JEFF", "LAMA", "LAUD", "LAWR",
    "LEE",  "LIME", "LOWN", "MACO", "MDSN", "MRGO", "MARI", "MRSH", "MOBI", "MNRO",
    "MGMY", "MORG", "PERR", "PICK", "PIKE", "RAND", "RSSL", "SCLR", "SHEL", "SUMT",
    "TDEG", "TPOO", "TUSC", "WLKR", "WASH", "WLCX", "WINS",
]

# US states (no AL since the AL op is in JEFF and won't send/work the AL state mult
# explicitly; AZ skipped because none of the chosen call pool will hit it).
US_STATES = ["AZ", "CA", "CO", "FL", "GA", "IL", "MA", "NY", "OH", "TX",
             "VA", "WA", "WI", "TN", "MI"]
PROVINCES = ["ON", "QC", "BC", "AB", "MB", "NS", "NB", "SK"]

# (band, mode) -> (frequency kHz, mode string stored in the QSO record)
SLOT = {
    ("20m", "CW"):  (14045, "CW"),
    ("40m", "CW"):  (7045,  "CW"),
    ("15m", "CW"):  (21045, "CW"),
    ("10m", "CW"):  (28045, "CW"),
    ("80m", "SSB"): (3810,  "SSB"),
    ("40m", "SSB"): (7230,  "SSB"),
    ("20m", "SSB"): (14250, "SSB"),
    ("15m", "SSB"): (21350, "SSB"),
    ("10m", "SSB"): (28450, "SSB"),
}

START = datetime(2026, 7, 25, 15, 0, 0)  # 1500Z Saturday July 25 2026


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
            "contest_file": "alqp.json",
            "mode": "",
            "name": "Alabama QSO Party",
            "type": "Alabama QSO Party",
            "version": "1.0.0",
            "year": 2026,
        },
        "created": "2026-07-25T15:00:00",
        "exchange_fields": [],
        "format": "ContestLogX",
        "format_version": "1.0",
        "modified": "2026-07-26T03:00:00",
        "qsos": qsos,
        "statistics": {"score": 0, "total_points": 0, "total_qsos": len(qsos)},
        "station": {"callsign": station, "operator": "Steve", "location": location},
    }
    path.write_text(json.dumps(doc, indent=4) + "\n")
    print(f"wrote {path.name}: {len(qsos)} QSOs")


def build_instate():
    """AL station in Jefferson county (JEFF). multsPerMode scoring.

    Phase A  15 AL counties        20m CW   15 x 2 = 30 pts, 15 CW county mults
    Phase B  10 US states          40m CW   10 x 2 = 20 pts, 10 CW state mults
    Phase C   8 CA provinces       40m SSB   8 x 2 = 16 pts,  8 SSB prov mults
    Phase D   5 DX                 15m CW    5 x 2 = 10 pts,  5 CW DXCC mults
    Phase E   3 dupes (Phase A)    20m CW    0 pts,           0 mults
    Phase F   4 counties from      80m SSB   4 x 2 =  8 pts,  4 SSB county mults
             Phase A on SSB                                   (new since multsPerMode)
    Phase G   1 DC station         20m CW    1 x 2 =  2 pts,  0 new mults
             (DC alias to MDC,                                 (MDC counted once
              MDC mult earned via                              under CW)
              the MD QSO in Phase B)
    Phase H   1 MD station         20m CW    1 x 2 =  2 pts,  1 CW MDC mult
             (MD also aliases to                              (MDC is new on CW
              MDC; sent before                                until now)
              the Phase G DC)

    Plus automaticMultipliers (multsPerMode): AL is credited once on every mode
    where the op worked an AL inStateMult. CW has AL counties worked (Phase A),
    SSB has AL counties worked (Phase F), so AL is credited on both modes for
    +2 auto mults.

    Actually swap so MD comes first (Phase G), DC after (Phase H, no new mult).
    Total: 47 QSOs, 88 pts, 42 QSO mults + 2 auto = 44 mults -> 88 * 44 = 3872

    Actual via engine: 86 pts (43 valid QSOs × 2) * 44 mults = 3784
    """
    # Pool: 15 counties + 10 states + 8 prov + 5 DX + 3 dupes (reuse) + 4 new
    # county-on-SSB calls + 2 (MD, DC) = 44 unique calls
    p = make_pool(seed=725, us=15 + 10 + 3 + 4 + 2, canada=8, intl=5)
    us, ca, dx = p["US"], p["CA"], p["DX"]

    # AL counties to work: skip JEFF (operator's own county)
    workable = [c for c in AL_COUNTIES if c != "JEFF"]

    qsos, t, idx = [], START, 1

    def add(call, band, mode, rst, exch_r):
        nonlocal t, idx
        qsos.append(qso(idx, t, call, band, mode, rst, "JEFF", exch_r))
        t += timedelta(minutes=2)
        idx += 1

    phaseA_calls = us[:15]
    for call, county in zip(phaseA_calls, workable[:15]):           # A
        add(call, "20m", "CW", "599", county)

    # Phase B: US states. Force MD into the first slot (Phase G/H will hit
    # DC after, which aliases to MDC -> already worked, no new mult).
    # The MD/DC ordering test exercises namedMultAliases.
    phaseB_calls = us[15:25]
    phaseB_states = ["MD"] + US_STATES[:9]
    for call, st in zip(phaseB_calls, phaseB_states):               # B
        add(call, "40m", "CW", "599", st)

    for call, prov in zip(ca, PROVINCES):                            # C
        add(call, "40m", "SSB", "59", prov)

    for call in dx:                                                  # D
        add(call, "15m", "CW", "599", "DX")

    for call, county in zip(phaseA_calls[:3], workable[:3]):         # E (dupes)
        add(call, "20m", "CW", "599", county)

    phaseF_calls = us[25:29]
    for call, county in zip(phaseF_calls, workable[:4]):             # F
        add(call, "80m", "SSB", "59", county)

    # Phase G: a DC station - alias to MDC, but MDC already worked under CW
    # (via the MD QSO in Phase B) so no new mult.
    add(us[29], "20m", "CW", "599", "DC")                            # G

    # Total points (non-dupe): (15 + 10 + 8 + 5 + 4 + 1) * 2 = 86
    # QSO mults: CW=(15 counties + 10 states + 5 DXCC)=30 ; SSB=(8 prov + 4 counties)=12 ; total 42
    # Auto mults: AL credited on CW and SSB (both modes have AL inStateMults worked) = +2
    # Total mults: 44
    # Score: 86 * 44 = 3784

    write_log(
        TEST_LOGS / "test_alqp_instate.clx",
        station=gc.generate_us_callsign(),
        location={"state": "AL", "cq_zone": 4, "itu_zone": 8},
        categories={
            "userPrompt_stationType": "AL",
            "userPrompt_operatingClass": "AL_SO",
            "userPrompt_powerCategory": "LOW",
            "userPrompt_modeCategory": "MIXED",
            "userPrompt_myExchange": "JEFF",
        },
        qsos=qsos,
    )
    return 86 * 44


def build_outofstate():
    """Non-AL station in Tennessee. multsPerMode scoring.

    Phase A  25 AL counties (CW)   20m CW   25 x 2 = 50 pts, 25 CW county mults
    Phase B  15 AL counties (CW)   40m CW   15 x 2 = 30 pts, 15 CW county mults
    Phase C  20 AL counties (SSB)  40m SSB  20 x 2 = 40 pts, 20 SSB county mults
    Phase D  10 AL counties (SSB)  80m SSB  10 x 2 = 20 pts, 10 SSB county mults
             (counties[0:10] from Phase A - already worked on CW, new on SSB)
    Phase E   5 dupes (Phase A)    20m CW    0 pts,           0 mults

    Total: 75 QSOs, 140 pts, 70 mults -> 140 * 70 = 9800
    """
    # Pool: 25 (A) + 15 (B) + 20 (C) + 10 (D) + 5 dupes
    p = make_pool(seed=726, us=25 + 15 + 20 + 10 + 5, canada=0, intl=0)
    us = p["US"]

    qsos, t, idx = [], START, 1

    def add(call, band, mode, rst, exch_r):
        nonlocal t, idx
        qsos.append(qso(idx, t, call, band, mode, rst, "TN", exch_r))
        t += timedelta(minutes=2)
        idx += 1

    phaseA_calls = us[:25]
    for call, county in zip(phaseA_calls, AL_COUNTIES[:25]):         # A
        add(call, "20m", "CW", "599", county)

    for call, county in zip(us[25:40], AL_COUNTIES[25:40]):          # B
        add(call, "40m", "CW", "599", county)

    for call, county in zip(us[40:60], AL_COUNTIES[40:60]):          # C
        add(call, "40m", "SSB", "59", county)

    # Phase D: counties[0:10] from Phase A - already worked on CW, new on SSB
    for call, county in zip(us[60:70], AL_COUNTIES[:10]):            # D
        add(call, "80m", "SSB", "59", county)

    for call, county in zip(phaseA_calls[:5], AL_COUNTIES[:5]):      # E (dupes)
        add(call, "20m", "CW", "599", county)

    # Score: (25+15+20+10) * 2 = 140 pts (5 dupes = 0)
    # Mults: CW=(25+15)=40 counties; SSB=(20+10)=30 counties; total 70
    # 140 * 70 = 9800

    write_log(
        TEST_LOGS / "test_alqp_outofstate.clx",
        station=gc.generate_us_callsign(),
        location={"state": "TN", "cq_zone": 4, "itu_zone": 8},
        categories={
            "userPrompt_stationType": "WVE",
            "userPrompt_operatingClass": "OUT_SO",
            "userPrompt_powerCategory": "LOW",
            "userPrompt_modeCategory": "MIXED",
            "userPrompt_myExchange": "TN",
        },
        qsos=qsos,
    )
    return 140 * 70


if __name__ == "__main__":
    s_in = build_instate()
    s_out = build_outofstate()
    print(f"expected in-state score:     {s_in}")
    print(f"expected out-of-state score: {s_out}")
