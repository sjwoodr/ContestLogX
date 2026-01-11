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
        return None
    except Exception as e:
        print(f"  ✗ ERROR running test '{test_name}': {e}")
        return None
    
    # Extract claimed score from debug log
    if not os.path.exists(debug_log):
        print(f"  ✗ ERROR: No debug log generated for test '{test_name}'")
        return None
    
    try:
        with open(debug_log, 'r') as f:
            log_content = f.read()
    except Exception as e:
        print(f"  ✗ ERROR reading debug log for test '{test_name}': {e}")
        return None
    
    # Search for TEST MODE score line
    pattern = r"TEST MODE: Log fully loaded\. CLAIMED_SCORE=(\d+)"
    matches = re.findall(pattern, log_content)
    
    if not matches:
        print(f"  ✗ ERROR: Could not find CLAIMED_SCORE in debug log for test '{test_name}'")
        print(f"     Last 500 chars of debug log:")
        print(f"     {log_content[-500:]}")
        return None
    
    # Return the last score found (in case of multiple runs)
    return int(matches[-1])

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
        actual_score = run_test(str(clx_path), str(log_path), test_name)
        
        if actual_score is None:
            errors += 1
            print()
            continue
        
        # Compare scores
        if actual_score == expected_score:
            print(f"  ✓ PASS: Claimed score = {actual_score}")
            passed += 1
        else:
            print(f"  ✗ FAIL: Expected {expected_score}, got {actual_score}")
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
