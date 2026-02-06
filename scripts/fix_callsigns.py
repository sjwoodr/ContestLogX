#!/usr/bin/env python3
"""
Fix callsigns in test log files by removing invalid trailing digits.
Valid amateur radio callsigns follow patterns like:
- W3F (area digit followed by letters)
- K1AB
- AA1BC
But NOT:
- W3F673 (invalid - has trailing digits)
- W2E330 (invalid - has trailing digits)
"""

import json
import re
import sys
from pathlib import Path


def fix_callsign(callsign):
    r"""
    Remove trailing digits from a callsign to make it valid.

    Valid callsign format is typically: [A-Z]{1,2}\d[A-Z]{1,4}
    Examples: W3F, K1AB, AA1BC

    This function removes any digits that appear after the suffix letters,
    while preserving the original prefix.

    Examples:
    - W3F673 -> W3F (keep W3 prefix)
    - K1AB234 -> K1AB (keep K1 prefix)
    """
    # Match callsign pattern: prefix letters + one digit + suffix letters + (invalid trailing digits)
    # We want to keep everything up to and including the suffix letters, but remove trailing digits
    match = re.match(r'^([A-Z]{1,2}\d[A-Z]{1,4})\d*$', callsign)
    if match:
        return match.group(1)
    return callsign


def fix_log_file(input_file, output_file=None):
    """
    Fix callsigns in a .clx log file ensuring uniqueness.
    If output_file is None, overwrites the input file.
    """
    if output_file is None:
        output_file = input_file

    # Read the log file
    print(f"Reading {input_file}...")
    with open(input_file, 'r') as f:
        log_data = json.load(f)

    # Track used callsigns to ensure uniqueness
    used_callsigns = set()

    # First pass: collect station callsign (should not be changed)
    if 'station' in log_data and 'callsign' in log_data['station']:
        used_callsigns.add(log_data['station']['callsign'])

    # Fix callsigns in QSOs
    fixed_count = 0
    duplicate_fixes = 0

    if 'qsos' in log_data:
        for qso in log_data['qsos']:
            if 'callsign' in qso:
                original = qso['callsign']
                fixed = fix_callsign(original)

                # If this callsign was already used, extend the suffix to make it unique
                if fixed in used_callsigns:
                    # Parse the callsign to extend it
                    match = re.match(r'^([A-Z]{1,2})(\d)([A-Z]+)$', fixed)
                    if match:
                        prefix = match.group(1)
                        digit = match.group(2)
                        suffix = match.group(3)

                        # Try adding letters to the suffix
                        for extra_letter in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
                            new_call = prefix + digit + suffix + extra_letter
                            if new_call not in used_callsigns:
                                fixed = new_call
                                duplicate_fixes += 1
                                break

                if original != fixed:
                    qso['callsign'] = fixed
                    fixed_count += 1
                    if fixed_count <= 5:  # Show first 5 examples
                        print(f"  Fixed: {original} -> {fixed}")

                used_callsigns.add(fixed)

    print(f"Fixed {fixed_count} callsigns ({duplicate_fixes} extended to avoid duplicates)")

    # Write the fixed log file
    print(f"Writing {output_file}...")
    with open(output_file, 'w') as f:
        json.dump(log_data, f, indent=2)

    print("Done!")


def main():
    if len(sys.argv) < 2:
        print("Usage: fix_callsigns.py <input_file> [output_file]")
        print("If output_file is not specified, the input file will be overwritten.")
        sys.exit(1)

    input_file = Path(sys.argv[1])
    if not input_file.exists():
        print(f"Error: File not found: {input_file}")
        sys.exit(1)

    output_file = Path(sys.argv[2]) if len(sys.argv) > 2 else input_file

    fix_log_file(input_file, output_file)


if __name__ == "__main__":
    main()
