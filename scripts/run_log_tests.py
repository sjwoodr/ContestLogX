#!/usr/bin/env python3
"""
Automated test runner for ContestLogX contest logs.
Reads test specifications from test_logs/automated_tests.json and validates
that CLX calculates the correct score for each test log.
"""

import json
import subprocess
import sys
import os
import re
from pathlib import Path

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
    """Validate that multiplier details match claimed counts."""
    # Remove timestamp/logger prefixes to normalize the content
    # Lines look like: [2026-01-11 11:47:05.777] MainWindow: Named Multipliers: 8
    normalized = re.sub(r'^\[.*?\]\s+\w+:\s+', '', log_content, flags=re.MULTILINE)

    # Extract the summary section (between "SCORING SUMMARY" and "MULTIPLIER DETAILS")
    summary_match = re.search(r'SCORING SUMMARY.*?(?=MULTIPLIER DETAILS)', normalized, re.DOTALL)
    if not summary_match:
        print(f"  ⚠ WARNING: Could not find SCORING SUMMARY section")
        return True  # Don't fail test, just warn

    summary = summary_match.group(0)

    # Extract claimed multiplier counts
    claimed_mults = {}

    # Look for patterns like "Named Multipliers:        8"
    named_match = re.search(r'Named Multipliers:\s+(\d+)', summary)
    if named_match:
        claimed_mults['Named Multipliers'] = int(named_match.group(1))

    # Look for "DXCC Multipliers:         2"
    dxcc_match = re.search(r'DXCC Multipliers:\s+(\d+)', summary)
    if dxcc_match:
        claimed_mults['DXCC Entities'] = int(dxcc_match.group(1))

    # Look for "Grid Square Multipliers:   3"
    grid_match = re.search(r'Grid Square Multipliers:\s+(\d+)', summary)
    if grid_match:
        claimed_mults['Grid Squares'] = int(grid_match.group(1))

    # Look for "Call Prefix Multipliers:   5"
    prefix_match = re.search(r'Call Prefix Multipliers:\s+(\d+)', summary)
    if prefix_match:
        claimed_mults['Call Prefixes'] = int(prefix_match.group(1))

    # For callsign multipliers, infer from score calculation
    # Score = Points × Multipliers, so check if callsigns are mentioned in details
    points_match = re.search(r'Contact Points:\s+(\d+)', summary)
    score_match = re.search(r'CLAIMED SCORE:\s+(\d+)', normalized)
    if points_match and score_match and not claimed_mults:
        contact_points = int(points_match.group(1))
        claimed_score = int(score_match.group(1))
        if contact_points > 0 and claimed_score % contact_points == 0:
            # Check if details section has "Callsigns" (meaning this is a callsign multiplier contest)
            details_check = re.search(r'MULTIPLIER DETAILS.*?Callsigns', normalized, re.DOTALL)
            if details_check:
                inferred_mults = claimed_score // contact_points
                claimed_mults['Callsigns'] = inferred_mults

    if not claimed_mults:
        # No multiplier counts found - might be a contest without multipliers
        return True

    # Extract the multiplier details section
    details_match = re.search(r'MULTIPLIER DETAILS\s*-+\s*(.*?)\s*=+', normalized, re.DOTALL)
    if not details_match:
        print(f"  ✗ MULTIPLIER ERROR: MULTIPLIER DETAILS section not found or empty")
        return False

    details = details_match.group(1)

    # Count multipliers in details section
    # Format is like: "Named Multipliers - CW (Worked: 4)"
    # followed by "*HI  *NC  *NY  *ON"

    actual_mults = {}

    # Find all multiplier sections
    for category_name in claimed_mults.keys():
        # Match lines like "Named Multipliers - CW (Worked: 4)" or "Named Multipliers (Worked: 4)"
        # Also match "DXCC Entities - SSB (Worked: 1)", "Grid Squares - 2m (Worked: 2)"
        # Also match "Callsigns (Worked: 40)" for callsign multipliers
        pattern = rf'{re.escape(category_name)}.*?\(Worked:\s*(\d+)\)'
        matches = re.findall(pattern, details)

        if matches:
            # Sum all the counts for this category (e.g., CW + SSB for multsPerMode)
            total = sum(int(m) for m in matches)
            actual_mults[category_name] = total

    # Validate that claimed counts match actual counts
    validation_passed = True
    for category, claimed_count in claimed_mults.items():
        actual_count = actual_mults.get(category, 0)
        if actual_count != claimed_count:
            print(f"  ✗ MULTIPLIER ERROR: {category}: claimed {claimed_count}, found {actual_count} in details")
            validation_passed = False

    if validation_passed:
        mult_summary = ", ".join([f"{cat}: {count}" for cat, count in claimed_mults.items()])
        print(f"  ✓ Multipliers validated: {mult_summary}")

    return validation_passed

def run_test(clx_path, log_file, test_name):
    """Run CLX with a test log and extract the claimed score from debug log."""
    debug_log = "clx_debug.log"

    # Remove old debug log
    if os.path.exists(debug_log):
        os.remove(debug_log)

    # Run CLX in test mode
    cmd = [clx_path, "--debug", "--log", log_file, "--test-only"]

    try:
        # Run with timeout to avoid hanging (large logs may take 20+ seconds)
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired:
        print(f"  ✗ TIMEOUT: Test '{test_name}' exceeded 60 second timeout")
        return None, None
    except Exception as e:
        print(f"  ✗ ERROR running test '{test_name}': {e}")
        return None, None

    # Extract claimed score from debug log
    if not os.path.exists(debug_log):
        print(f"  ✗ ERROR: No debug log generated for test '{test_name}'")
        return None, None

    try:
        with open(debug_log, 'r') as f:
            log_content = f.read()
    except Exception as e:
        print(f"  ✗ ERROR reading debug log for test '{test_name}': {e}")
        return None, None

    # Search for TEST MODE score line
    pattern = r"TEST MODE: Log fully loaded\. CLAIMED_SCORE=(\d+)"
    matches = re.findall(pattern, log_content)

    if not matches:
        print(f"  ✗ ERROR: Could not find CLAIMED_SCORE in debug log for test '{test_name}'")
        print(f"     Last 500 chars of debug log:")
        print(f"     {log_content[-500:]}")
        return None, None

    # Validate multipliers
    mult_valid = validate_multipliers(log_content, test_name)

    # Return the last score found (in case of multiple runs) and multiplier validation result
    return int(matches[-1]), mult_valid

def main():
    """Main test runner."""
    # Get paths
    script_dir = Path(__file__).parent
    repo_root = script_dir.parent
    config_file = repo_root / "test_logs" / "automated_tests.json"
    clx_path = repo_root / "clx"
    
    if not clx_path.exists():
        print(f"Error: CLX executable not found: {clx_path}")
        sys.exit(1)
    
    # Load test configuration
    config = load_test_config(config_file)
    
    if "tests" not in config:
        print("Error: No 'tests' array in automated_tests.json")
        sys.exit(1)
    
    tests = config["tests"]
    if not tests:
        print("Error: No tests defined in automated_tests.json")
        sys.exit(1)
    
    # Change to repo root for relative paths
    os.chdir(repo_root)
    
    # Run tests
    passed = 0
    failed = 0
    errors = 0
    
    print("=" * 70)
    print("ContestLogX Automated Contest Log Tests")
    print("=" * 70)
    print()
    
    for i, test in enumerate(tests, 1):
        test_name = test.get("name", f"Test {i}")
        log_file = test.get("log_file")
        expected_score = test.get("expected_score")
        
        if not log_file or expected_score is None:
            print(f"✗ Test {i}: Invalid test configuration (missing log_file or expected_score)")
            errors += 1
            continue
        
        # Resolve log file path
        log_path = Path("test_logs") / log_file
        if not log_path.exists():
            print(f"✗ Test {i}: {test_name}")
            print(f"  Log file not found: {log_path}")
            errors += 1
            continue
        
        print(f"Test {i}: {test_name}")
        print(f"  Log: {log_file}")
        print(f"  Expected score: {expected_score}")

        # Run the test
        actual_score, mult_valid = run_test(str(clx_path), str(log_path), test_name)

        if actual_score is None:
            errors += 1
            print()
            continue

        # Compare scores and multipliers
        score_passed = (actual_score == expected_score)

        if score_passed and mult_valid:
            print(f"  ✓ PASS: Claimed score = {actual_score}")
            passed += 1
        else:
            if not score_passed:
                print(f"  ✗ FAIL: Expected score {expected_score}, got {actual_score}")
            if not mult_valid:
                print(f"  ✗ FAIL: Multiplier validation failed")
            failed += 1

        print()
    
    # Print summary
    print("=" * 70)
    print(f"Test Results: {passed} passed, {failed} failed, {errors} errors")
    print("=" * 70)
    
    if failed > 0 or errors > 0:
        sys.exit(1)
    else:
        print("All tests passed!")
        sys.exit(0)

if __name__ == "__main__":
    main()
