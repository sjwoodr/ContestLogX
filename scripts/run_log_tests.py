#!/usr/bin/env python3
"""
Automated test runner for ContestLogX contest logs.
Reads test specifications from test_logs/automated_tests.json and validates
that CLX calculates the correct score for each test log.

Runs tests in parallel using a thread pool (default: 4 workers).
Use --workers N to control parallelism, or --workers 1 for serial execution.
"""

import json
import subprocess
import sys
import os
import re
import time
import tempfile
import argparse
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed


def load_test_config(config_file):
    """Load automated test configuration from JSON."""
    try:
        with open(config_file, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"Error: Test configuration file not found: {config_file}")
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in {config_file}: {e}")
        sys.exit(1)


def validate_multipliers(log_content, test_name):
    """Validate that multiplier details match claimed counts.
    Returns (passed: bool, lines: list[str])
    """
    lines = []
    # Remove timestamp/logger prefixes to normalize the content
    # Lines look like: [2026-01-11 11:47:05.777] MainWindow: Named Multipliers: 8
    normalized = re.sub(r'^\[.*?\]\s+\w+:\s+', '', log_content, flags=re.MULTILINE)

    # Extract the summary section (between "SCORING SUMMARY" and "MULTIPLIER DETAILS")
    summary_match = re.search(r'SCORING SUMMARY.*?(?=MULTIPLIER DETAILS)', normalized, re.DOTALL)
    if not summary_match:
        lines.append(f"  ⚠ WARNING: Could not find SCORING SUMMARY section")
        return True, lines  # Don't fail test, just warn

    summary = summary_match.group(0)

    # Extract claimed multiplier counts
    claimed_mults = {}

    named_match = re.search(r'Named Multipliers:\s+(\d+)', summary)
    if named_match:
        claimed_mults['Named Multipliers'] = int(named_match.group(1))

    # Custom named mult labels (e.g., "CQ Zones:", "Prefectures:")
    # Look between "Contact Points" and "Score Calculation" for unknown mult lines
    mults_section = re.search(r'Contact Points:.*?Score Calculation:', summary, re.DOTALL)
    if mults_section:
        known_labels = {'Contact Points', 'Named Multipliers', 'DXCC Multipliers',
                        'Grid Square Multipliers', 'Call Prefix Multipliers',
                        'ITU Region Multipliers'}
        for m in re.finditer(r'^([A-Za-z][^:\n]+):\s+(\d+)\s*$', mults_section.group(0), re.MULTILINE):
            label = m.group(1).strip()
            if label not in known_labels:
                claimed_mults[label] = int(m.group(2))

    dxcc_match = re.search(r'DXCC Multipliers:\s+(\d+)', summary)
    if dxcc_match:
        claimed_mults['DXCC Entities'] = int(dxcc_match.group(1))

    grid_match = re.search(r'Grid Square Multipliers:\s+(\d+)', summary)
    if grid_match:
        claimed_mults['Grid Squares'] = int(grid_match.group(1))

    prefix_match = re.search(r'Call Prefix Multipliers:\s+(\d+)', summary)
    if prefix_match:
        claimed_mults['Call Prefixes'] = int(prefix_match.group(1))

    points_match = re.search(r'Contact Points:\s+(\d+)', summary)
    score_match = re.search(r'CLAIMED SCORE:\s+(\d+)', normalized)
    if points_match and score_match and not claimed_mults:
        contact_points = int(points_match.group(1))
        claimed_score = int(score_match.group(1))
        if contact_points > 0 and claimed_score % contact_points == 0:
            details_check = re.search(r'MULTIPLIER DETAILS.*?Callsigns', normalized, re.DOTALL)
            if details_check:
                inferred_mults = claimed_score // contact_points
                claimed_mults['Callsigns'] = inferred_mults

    if not claimed_mults:
        return True, lines

    # Extract the multiplier details section
    details_match = re.search(r'MULTIPLIER DETAILS\s*-+\s*(.*?)\s*=+', normalized, re.DOTALL)
    if not details_match:
        lines.append(f"  ✗ MULTIPLIER ERROR: MULTIPLIER DETAILS section not found or empty")
        return False, lines

    details = details_match.group(1)

    actual_mults = {}
    for category_name in claimed_mults.keys():
        pattern = rf'{re.escape(category_name)}.*?\(Worked:\s*(\d+)\)'
        matches = re.findall(pattern, details)
        if matches:
            actual_mults[category_name] = sum(int(m) for m in matches)

    validation_passed = True
    for category, claimed_count in claimed_mults.items():
        actual_count = actual_mults.get(category, 0)
        if actual_count != claimed_count:
            lines.append(f"  ✗ MULTIPLIER ERROR: {category}: claimed {claimed_count}, found {actual_count} in details")
            validation_passed = False

    if validation_passed:
        mult_summary = ", ".join([f"{cat}: {count}" for cat, count in claimed_mults.items()])
        lines.append(f"  ✓ Multipliers validated: {mult_summary}")

    return validation_passed, lines


def run_test(clx_path, log_file, test_name, index):
    """Run CLX with a test log and return (actual_score, mult_valid, output_lines, elapsed)."""
    import shutil
    lines = []

    with tempfile.NamedTemporaryFile(suffix='.log', delete=False) as tmp:
        debug_log = tmp.name

    # Per-test sandbox config dir so parallel workers don't race on the
    # user's real config (and so the user's real config is never touched
    # by `make test-logs` in any case). CLX's --config-dir flag seeds the
    # sandbox from the real config on first use, which is what we want
    # so tests inherit the user's identity / station info / decoder
    # defaults rather than running as a first-launch defaults instance.
    sandbox_dir = tempfile.mkdtemp(prefix='clx-test-sandbox-')

    try:
        cmd = [clx_path, "--debug", "--log", log_file, "--test-only",
               "--debug-log", debug_log, "--config-dir", sandbox_dir]

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        except subprocess.TimeoutExpired:
            lines.append(f"  ✗ TIMEOUT: Test '{test_name}' exceeded 60 second timeout")
            return None, None, lines, 0
        except Exception as e:
            lines.append(f"  ✗ ERROR running test '{test_name}': {e}")
            return None, None, lines, 0

        if not os.path.exists(debug_log):
            lines.append(f"  ✗ ERROR: No debug log generated for test '{test_name}'")
            return None, None, lines, 0

        try:
            with open(debug_log, 'r') as f:
                log_content = f.read()
        except Exception as e:
            lines.append(f"  ✗ ERROR reading debug log for test '{test_name}': {e}")
            return None, None, lines, 0

        pattern = r"TEST MODE: Log fully loaded\. CLAIMED_SCORE=(\d+)"
        matches = re.findall(pattern, log_content)

        if not matches:
            lines.append(f"  ✗ ERROR: Could not find CLAIMED_SCORE in debug log for test '{test_name}'")
            lines.append(f"     Last 500 chars of debug log:")
            lines.append(f"     {log_content[-500:]}")
            return None, None, lines, 0

        mult_valid, mult_lines = validate_multipliers(log_content, test_name)
        lines.extend(mult_lines)

        return int(matches[-1]), mult_valid, lines, 0

    finally:
        try:
            os.unlink(debug_log)
        except OSError:
            pass
        try:
            shutil.rmtree(sandbox_dir, ignore_errors=True)
        except OSError:
            pass


def run_test_timed(clx_path, log_file, test_name, index):
    """Wrapper that times the test run."""
    t_start = time.monotonic()
    actual_score, mult_valid, lines, _ = run_test(clx_path, log_file, test_name, index)
    elapsed = time.monotonic() - t_start
    return index, test_name, log_file, actual_score, mult_valid, lines, elapsed


def main():
    parser = argparse.ArgumentParser(description="ContestLogX automated log test runner")
    parser.add_argument("--workers", type=int, default=4,
                        help="Number of parallel test workers (default: 4)")
    args = parser.parse_args()

    script_dir = Path(__file__).parent
    repo_root = script_dir.parent
    config_file = repo_root / "test_logs" / "automated_tests.json"
    clx_path = repo_root / "clx"

    if not clx_path.exists():
        print(f"Error: CLX executable not found: {clx_path}")
        sys.exit(1)

    config = load_test_config(config_file)

    if "tests" not in config:
        print("Error: No 'tests' array in automated_tests.json")
        sys.exit(1)

    tests = config["tests"]
    if not tests:
        print("Error: No tests defined in automated_tests.json")
        sys.exit(1)

    os.chdir(repo_root)

    print("=" * 70)
    print("ContestLogX Automated Contest Log Tests")
    print(f"Running {len(tests)} tests with {args.workers} worker(s)")
    print("=" * 70)
    print()

    # Validate test configs and build the work list
    work = []
    pre_errors = 0
    for i, test in enumerate(tests, 1):
        test_name = test.get("name", f"Test {i}")
        log_file = test.get("log_file")
        expected_score = test.get("expected_score")

        if not log_file or expected_score is None:
            print(f"✗ Test {i}: Invalid test configuration (missing log_file or expected_score)")
            pre_errors += 1
            continue

        log_path = Path("test_logs") / log_file
        if not log_path.exists():
            print(f"✗ Test {i}: {test_name}")
            print(f"  Log file not found: {log_path}")
            print()
            pre_errors += 1
            continue

        work.append((i, test_name, str(log_path), expected_score))

    passed = 0
    failed = 0
    errors = pre_errors

    suite_start = time.monotonic()

    expected_by_idx = {idx: es for (idx, tn, lp, es) in work}

    with ThreadPoolExecutor(max_workers=args.workers) as executor:
        futures = {
            executor.submit(run_test_timed, str(clx_path), log_path, test_name, idx): idx
            for (idx, test_name, log_path, expected_score) in work
        }

        for future in as_completed(futures):
            idx, test_name, log_file, actual_score, mult_valid, extra_lines, elapsed = future.result()
            expected_score = expected_by_idx[idx]

            print(f"Test {idx}: {test_name}")
            print(f"  Log: {log_file}")
            print(f"  Expected score: {expected_score}")

            for line in extra_lines:
                print(line)

            if actual_score is None:
                errors += 1
            else:
                score_passed = (actual_score == expected_score)
                if score_passed and mult_valid:
                    print(f"  ✓ PASS: Claimed score = {actual_score}  ({elapsed:.2f}s)")
                    passed += 1
                else:
                    if not score_passed:
                        print(f"  ✗ FAIL: Expected score {expected_score}, got {actual_score}  ({elapsed:.2f}s)")
                    if not mult_valid:
                        print(f"  ✗ FAIL: Multiplier validation failed")
                    failed += 1

            print()

    suite_elapsed = time.monotonic() - suite_start
    print("=" * 70)
    print(f"Test Results: {passed} passed, {failed} failed, {errors} errors  (total {suite_elapsed:.2f}s)")
    print("=" * 70)

    if failed > 0 or errors > 0:
        sys.exit(1)
    else:
        print("All tests passed!")
        sys.exit(0)


if __name__ == "__main__":
    main()
