#!/usr/bin/env python3
"""Generate 150 QSOs for SPDX contest test log (DX station working SP stations)."""

import json
import random
from datetime import datetime, timedelta

random.seed(73)

CALLSIGNS = [
    "SO2GF","SP1UA","SN7KM","SO9LNG","3Z6NV","SN3PM","SP7NLF","HF7UC","SO1JYS","3Z7NQ",
    "SN5AZ","SN8NG","SO6BH","SO2DKJ","SN6IS","SO2ROG","SP1PJ","3Z9WF","SQ4VOH","SP4XS",
    "SP1NJ","SN2VI","SP4LQT","HF8EN","SQ8UE","SN2PC","SP7AYR","3Z5AGO","SP3QY","SP5ML",
    "SQ2CQZ","SQ3JLP","3Z2UD","SN9QAX","SO9NQ","SO4RCE","SP7SO","SQ3UU","SO9OL","SN6GQM",
    "SP6EH","SO7GGJ","SP7SI","SO5PPK","HF1BEH","SN3PT","HF4RZJ","SP1NMR","SQ2LY","SO1VH",
    "SN9KL","SO3RI","SQ4RBT","SP7PP","SQ6ZP","3Z4TCO","SO4DL","SP9WCJ","HF2IKH","HF7INX",
    "SQ7UMT","SN9QIF","SN8CIB","SP5BU","SO8LK","SP6OT","SQ4PQA","3Z2RF","SN2KO","SQ8RKT",
    "SQ1CJN","SN3FF","SQ7AY","3Z6VO","HF1TPN","SN1PQP","SO7DIX","SN2KBS","3Z4KI","SO1DQZ",
    "SQ7IAZ","SN6YFF","HF9UKO","SQ3HO","SQ1JFS","HF3CVW","SN8TX","HF2FWC","SP6GUQ","SO3MNB",
    "SQ9YW","SP6IM","SP1GC","SQ3BV","3Z6YPS","SP1OD","3Z8SMV","SO6KP","SQ3UW","SP9UH",
    "SQ6GU","SO2OL","SN1AW","SN6KIA","SO5YWN","SQ3LTX","SP6XSM","3Z3WT","SQ3FCU","SP6WW",
    "HF9PGL","HF8SJ","3Z2IKA","SQ5NB","SO2LCR","HF5VL","HF2RC","HF6XA","3Z3CMV","SN6ZA",
    "HF2NF","SP9QZ","SO4MD","SO3AER","SO7YDQ","3Z7PH","SO9QT","SP5ZIG","SO1JTO","SN3GPF",
    "SO4MUI","SQ4SN","SN5GT","3Z3VTM","SQ3PG","HF9VQS","SN8XH","3Z2QG","HF9YP","SQ7WTM",
    "3Z9VQW","HF3XKK","SP5FWW","SP9TM","SP5SU","3Z2DRQ","3Z4QA","SN7AN","SQ7TA","SQ4LBN",
]

PROVINCES = ["B","C","D","F","G","J","K","L","M","O","P","R","S","U","W","Z"]
BANDS = ["160m","80m","40m","20m","15m","10m"]

# Frequency ranges: (band, mode) -> (low, high)
FREQ_RANGES = {
    ("160m","CW"):  (1800, 1850),
    ("160m","USB"): (1850, 1900),
    ("80m","CW"):   (3500, 3560),
    ("80m","USB"):  (3750, 3800),
    ("40m","CW"):   (7000, 7040),
    ("40m","USB"):  (7150, 7250),
    ("20m","CW"):   (14000, 14060),
    ("20m","USB"):  (14200, 14300),
    ("15m","CW"):   (21000, 21060),
    ("15m","USB"):  (21250, 21350),
    ("10m","CW"):   (28000, 28060),
    ("10m","USB"):  (28400, 28500),
}

# Read existing CLX file
with open("/home/steve/src/other/ContestLogX/test_logs/test_spdx_wve.clx") as f:
    clx = json.load(f)

# We keep QSO #1 (SP1DX, 20m USB, province B) as-is
# Track multipliers: existing QSO already has 20m/B
worked_mults = {}  # (band, province) -> True
worked_mults[("20m", "B")] = True

# Build 150 QSOs with good province/band distribution
# First ensure all 16 provinces appear across different bands
# Assign bands round-robin style, provinces distributed
qso_assignments = []

# Phase 1: Ensure each province appears at least once on different bands
for i, prov in enumerate(PROVINCES):
    band = BANDS[i % len(BANDS)]
    mode = "CW" if random.random() < 0.5 else "USB"
    qso_assignments.append((band, mode, prov))

# Phase 2: Fill remaining 134 QSOs with random distribution
for _ in range(150 - len(PROVINCES)):
    band = random.choice(BANDS)
    mode = "CW" if random.random() < 0.5 else "USB"
    prov = random.choice(PROVINCES)
    qso_assignments.append((band, mode, prov))

# Shuffle to mix them up (but use seeded random)
random.shuffle(qso_assignments)

# Generate QSOs
start_time = datetime(2026, 4, 4, 15, 0, 0)  # Contest start
current_time = start_time

qsos = list(clx["qsos"])  # Keep existing QSO #1
total_mults = 1  # Already have 20m/B from QSO #1

for i, (band, mode, province) in enumerate(qso_assignments):
    callsign = CALLSIGNS[i]
    serial = i + 2  # Start from 2 (QSO 1 already exists)

    # Increment time by 2-5 minutes
    delta_minutes = random.randint(2, 5)
    current_time += timedelta(minutes=delta_minutes)

    # Generate frequency
    freq_low, freq_high = FREQ_RANGES[(band, mode)]
    frequency = random.randint(freq_low, freq_high)

    # RST depends on mode
    rst = "599" if mode == "CW" else "59"

    # Multiplier tracking
    mult_key = (band, province)
    if mult_key not in worked_mults:
        worked_mults[mult_key] = True
        mult_count = 1
        total_mults += 1
    else:
        mult_count = 0

    qso = {
        "band": band,
        "callsign": callsign,
        "duplicate": False,
        "dxcc_count": 0,
        "exchange_fields": {
            "CALL": callsign,
            "EXCHr": province,
            "EXCHs": "FL",
            "NAMEs": "Steve",
            "RSTr": rst,
            "RSTs": rst,
            "SNs": str(serial)
        },
        "frequency": frequency,
        "grid_square_count": 0,
        "id": serial,
        "mode": mode,
        "multiplier_count": mult_count,
        "points": 3,
        "rst_received": rst,
        "rst_sent": rst,
        "serial": serial,
        "timestamp": current_time.strftime("%Y-%m-%dT%H:%M:%SZ")
    }
    qsos.append(qso)

# Update CLX
total_qsos = len(qsos)
total_points = total_qsos * 3
score = total_points * total_mults

clx["qsos"] = qsos
clx["statistics"] = {
    "score": score,
    "total_points": total_points,
    "total_qsos": total_qsos
}
clx["modified"] = "2026-04-04T15:00:00"

with open("/home/steve/src/other/ContestLogX/test_logs/test_spdx_wve.clx", "w") as f:
    json.dump(clx, f, indent=4)

# Print summary
print(f"Total QSOs: {total_qsos}")
print(f"Total points: {total_points}")
print(f"Total multipliers: {total_mults}")
print(f"Score: {score}")

# Show mult distribution
from collections import Counter
band_mults = Counter()
prov_counts = Counter()
for (band, prov) in worked_mults:
    band_mults[band] += 1
    prov_counts[prov] += 1

print("\nMults per band:")
for band in BANDS:
    print(f"  {band}: {band_mults[band]}")

print(f"\nProvinces seen: {len(prov_counts)} / 16")
print(f"Provinces: {sorted(prov_counts.keys())}")
