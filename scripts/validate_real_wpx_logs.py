#!/usr/bin/env python3
"""
Validate CLX's CQ WPX scoring against real public Cabrillo logs.

Downloads (or reads cached) public WPX logs from cqwpx.com/publiclogs, converts
each one to CLX's .clx JSON format, runs CLX in --test-only mode, and compares
CLX's computed score against the operator's CLAIMED-SCORE in the Cabrillo
header. A match means our scoring agrees with whatever logger produced the
log (N1MM, Win-Test, WriteLog, etc.) - which is the closest thing we have to
a ground-truth comparison without access to the official adjudicator.

Usage:
    python3 scripts/validate_real_wpx_logs.py [--cache-dir DIR] [--keep-clx]

Adds CACHE_DIR (default: /tmp/wpx_real_logs) so the same logs aren't
re-downloaded every run. --keep-clx leaves the converted CLX files on disk
under test_logs/_real/ for manual inspection.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import urllib.request
from datetime import datetime
from pathlib import Path


# Each entry: (callsign_lowercase, year, mode_path, expected_claimed_score, note)
# Picked deliberately to span small/medium/large logs and exotic prefixes.
LOGS = [
    ("4o6zd",  "2024", "cw",   4233,    "Montenegro, EU, single-band 15m QRP, 53 QSOs (smallest sanity check)"),
    ("4l2m",   "2024", "cw",   629649,  "Georgia, AS, single-band 10m HIGH, ~600 QSOs"),
    ("3v8ss",  "2024", "cw",   1623300, "Tunisia, AF, all-band LOW TB-WIRES, ~1000 QSOs"),
    ("3d2sp",  "2024", "cw",   3702032, "Fiji, OC, single-band 15m HIGH (digit-led prefix), ~1600 QSOs"),
    ("3w9a",   "2024", "cw",   4051992, "Vietnam, AS, all-band LOW TB-WIRES, ~1900 QSOs (largest)"),
]

URL_PATTERN = "https://cqwpx.com/publiclogs/{year}{mode}/{call}.log"


def freq_to_band(freq_khz: int) -> str:
    """Convert a Cabrillo-format frequency (kHz, integer) to a band name."""
    if 1800 <= freq_khz <= 2000:   return "160m"
    if 3500 <= freq_khz <= 4000:   return "80m"
    if 7000 <= freq_khz <= 7300:   return "40m"
    if 10100 <= freq_khz <= 10150: return "30m"   # WARC, OOB for WPX
    if 14000 <= freq_khz <= 14350: return "20m"
    if 18068 <= freq_khz <= 18168: return "17m"   # WARC
    if 21000 <= freq_khz <= 21450: return "15m"
    if 24890 <= freq_khz <= 24990: return "12m"   # WARC
    if 28000 <= freq_khz <= 29700: return "10m"
    return ""


def parse_cabrillo(log_path: Path):
    """Return (header_dict, qsos_list)."""
    headers = {}
    qsos = []
    with log_path.open() as f:
        for line in f:
            line = line.rstrip("\r\n")
            if line.startswith("QSO:"):
                # Cabrillo QSO: freq mode date time mycall RSTs SNs theircall RSTr SNr [transmitter]
                parts = line[4:].split()
                if len(parts) < 10:
                    continue
                qsos.append({
                    "freq_khz": int(parts[0]),
                    "mode": parts[1],
                    "date": parts[2],          # yyyy-mm-dd
                    "time": parts[3],          # HHMM
                    "mycall": parts[4],
                    "rst_sent": parts[5],
                    "sn_sent": parts[6],
                    "theircall": parts[7],
                    "rst_rcvd": parts[8],
                    "sn_rcvd": parts[9],
                })
            elif line.startswith("X-QSO:"):
                continue                       # operator-marked dupe, skip
            elif ":" in line and not line.startswith(("START-OF-LOG", "END-OF-LOG")):
                key, _, value = line.partition(":")
                headers[key.strip().upper()] = value.strip()
    return headers, qsos


def cabrillo_to_clx(log_path: Path, out_path: Path):
    """Convert a Cabrillo log into CLX's JSON format. Returns (claimed_score,
    qsos_in_log, qsos_in_clx, category_band)."""
    headers, qsos = parse_cabrillo(log_path)
    callsign       = headers.get("CALLSIGN", "")
    contest_tag    = headers.get("CONTEST", "")
    claimed_score  = int(headers.get("CLAIMED-SCORE", "0") or "0")
    cat_band       = headers.get("CATEGORY-BAND", "ALL").upper()
    operator_name  = headers.get("NAME", "Operator")
    cat_mode       = headers.get("CATEGORY-MODE", "CW").upper()
    cat_op         = headers.get("CATEGORY-OPERATOR", "SINGLE-OP")

    # Map Cabrillo CONTEST tag back to userPrompt contestMode used by cqwpx.json.
    contest_mode = "CW" if contest_tag.endswith("-CW") else "SSB"

    # Single-band entries: per CQ WPX rule XI.B, only QSOs on the declared band
    # count toward score. Filter out anything else so CLX scores apples-to-apples.
    band_filter = None
    if cat_band not in ("ALL", "ALL-BAND", ""):
        m = re.match(r"^(\d+)M", cat_band)
        if m:
            band_filter = m.group(1) + "m"

    clx_qsos = []
    skipped_band = 0
    for i, q in enumerate(qsos, 1):
        band = freq_to_band(q["freq_khz"])
        if band_filter is not None and band != band_filter:
            skipped_band += 1
            continue                            # off-band QSO for a single-band entry
        # Cabrillo time → ISO 8601
        try:
            ts = datetime.strptime(q["date"] + q["time"], "%Y-%m-%d%H%M")
        except ValueError:
            continue
        rst = "599" if contest_mode == "CW" else "59"
        clx_qsos.append({
            "band": band,
            "callsign": q["theircall"],
            "duplicate": False,
            "exchange_fields": {
                "CALL": q["theircall"],
                "RSTr": q["rst_rcvd"],
                "RSTs": q["rst_sent"],
                "SNs":  q["sn_sent"],
                "SNr":  q["sn_rcvd"],
            },
            "frequency": q["freq_khz"],
            "id": i,
            "mode": "CW" if q["mode"] == "CW" else "SSB",
            "points": 0,
            "rst_received": q["rst_rcvd"],
            "rst_sent":     q["rst_sent"],
            "serial": int(q["sn_sent"]) if q["sn_sent"].isdigit() else 0,
            "timestamp": ts.strftime("%Y-%m-%dT%H:%M:%SZ"),
        })

    log = {
        "contest": {
            "categories": {
                "userPrompt_contestMode": contest_mode,
                "userPrompt_operatingCategory": cat_op,
            },
            "contest_file": "cqwpx.json",
            "mode": contest_mode,
            "name": "CQ World-Wide WPX Contest",
            "type": "CQ World-Wide WPX Contest",
            "version": "1.0.0",
            "year": int(headers.get("CALLSIGN", "0000")[-4:].split()[0] or 2024) if False else 2024,
        },
        "created": "2024-05-25T00:00:00",
        "exchange_fields": [],
        "format": "ContestLogX",
        "format_version": "1.0",
        "modified": "2024-05-25T00:00:00",
        "qsos": clx_qsos,
        "station": {
            "callsign": callsign,
            "equipment": {"power": 100},
            "operator": operator_name,
        },
        "statistics": {"score": 0, "total_points": 0, "total_qsos": len(clx_qsos)},
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(log, indent=2))
    return claimed_score, len(qsos), len(clx_qsos), band_filter or "ALL", skipped_band


def run_clx(repo_root: Path, clx_path: Path):
    """Run CLX in --test-only mode and return its CLAIMED_SCORE."""
    binary = repo_root / "clx"
    if not binary.exists():
        sys.exit(f"CLX binary not found: {binary} (build first)")

    with tempfile.NamedTemporaryFile(suffix=".log", delete=False) as f:
        debug_log = f.name
    try:
        with tempfile.TemporaryDirectory() as sandbox:
            cmd = [str(binary), "--debug", "--log", str(clx_path), "--test-only",
                   "--debug-log", debug_log, "--config-dir", sandbox]
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
            if r.returncode != 0 and "TEST MODE" not in (r.stdout + r.stderr):
                # log briefly so we can see what went wrong
                pass
        with open(debug_log) as f:
            content = f.read()
    finally:
        try: os.unlink(debug_log)
        except OSError: pass

    matches = re.findall(r"TEST MODE: Log fully loaded\. CLAIMED_SCORE=(\d+)", content)
    if not matches:
        # As a fallback, capture the running-score's last "contestScore" log line
        m = re.findall(r"contestScore=(\d+)", content)
        if m: return int(m[-1])
        return None
    return int(matches[-1])


def download_log(cache_dir: Path, call: str, year: str, mode: str) -> Path:
    """Download a public log into cache_dir if not already there."""
    cache_dir.mkdir(parents=True, exist_ok=True)
    out = cache_dir / f"{call}.log"
    if out.exists() and out.stat().st_size > 100:
        return out
    url = URL_PATTERN.format(call=call, year=year, mode=mode)
    print(f"  downloading {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "ContestLogX-validator/1.0"})
    with urllib.request.urlopen(req, timeout=60) as resp:
        out.write_bytes(resp.read())
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache-dir", default="/tmp/wpx_real_logs",
                        help="Where to cache downloaded Cabrillo logs")
    parser.add_argument("--keep-clx", action="store_true",
                        help="Keep generated .clx files in test_logs/_real/")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    cache_dir = Path(args.cache_dir)
    clx_dir = repo_root / "test_logs" / "_real"

    print(f"{'Call':<8} {'QSOs':>6} {'Filtered':>9} {'Band':<5} "
          f"{'Claimed':>10} {'CLX':>10} {'Δ':>10} {'Match'}")
    print("-" * 90)
    all_match = True

    for call, year, mode, expected_score, note in LOGS:
        cabrillo = download_log(cache_dir, call, year, mode)
        clx_file = clx_dir / f"{call}.clx"

        claimed, n_log, n_clx, band, skipped = cabrillo_to_clx(cabrillo, clx_file)

        clx_score = run_clx(repo_root, clx_file)
        delta = (clx_score - claimed) if clx_score is not None else None
        match = (clx_score == claimed)
        all_match = all_match and (match or False)
        match_str = "✓" if match else "✗"
        clx_str = f"{clx_score}" if clx_score is not None else "?"
        delta_str = (f"{delta:+d}" if delta is not None else " - ")

        print(f"{call.upper():<8} {n_log:>6} {n_clx:>9} {band:<5} "
              f"{claimed:>10} {clx_str:>10} {delta_str:>10}  {match_str}  {note}")

        if not args.keep_clx:
            try: clx_file.unlink()
            except OSError: pass

    print()
    if all_match:
        print("All real-log claimed scores match CLX's calculation.")
        return 0
    else:
        print("One or more real-log scores diverged - investigate above.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
