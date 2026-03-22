#!/usr/bin/env python3
"""
Generate valid amateur radio callsigns for testing purposes.

Valid callsign patterns:
- US: W/K/N/A + digit (0-9) + 1-4 letters, or AA-AL prefix
- Canada: VE/VA/VO/VY + digit + 1-4 letters
- Mexico: XE + digit + 1-4 letters
- International: Various prefixes like G, DL, JA, etc.
"""

import random
import string
import argparse


def generate_us_callsign():
    """Generate a valid US callsign."""
    # Choose between single letter prefix (W, K, N, A) or two letter (AA-AL)
    if random.random() < 0.7:
        # Single letter prefix
        prefix = random.choice(['W', 'K', 'N', 'A'])
    else:
        # Two letter prefix (AA-AL are valid US prefixes)
        prefix = 'A' + random.choice(['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L'])

    # Area digit (0-9)
    digit = str(random.randint(0, 9))

    # Suffix letters (1-4 letters, typically 1-3 for most calls)
    suffix_length = random.choices([1, 2, 3, 4], weights=[10, 40, 45, 5])[0]
    suffix = ''.join(random.choices(string.ascii_uppercase, k=suffix_length))

    return prefix + digit + suffix


def generate_canadian_callsign():
    """Generate a valid Canadian callsign."""
    # Canadian prefixes: VE, VA, VO (Newfoundland), VY (Yukon/NWT)
    prefix = random.choice(['VE', 'VA', 'VO', 'VY'])

    # Area digit (1-9, with VE1-VE9 being most common)
    digit = str(random.randint(1, 9))

    # Suffix letters (typically 2-3 letters for Canadian calls)
    suffix_length = random.choices([2, 3], weights=[60, 40])[0]
    suffix = ''.join(random.choices(string.ascii_uppercase, k=suffix_length))

    return prefix + digit + suffix


def generate_mexican_callsign():
    """Generate a valid Mexican callsign."""
    # Mexican prefix: XE or XA
    prefix = random.choice(['XE', 'XA'])

    # Area digit (1-3 are most common)
    digit = str(random.randint(1, 3))

    # Suffix letters (typically 2-3 letters)
    suffix_length = random.choices([2, 3], weights=[50, 50])[0]
    suffix = ''.join(random.choices(string.ascii_uppercase, k=suffix_length))

    return prefix + digit + suffix


def generate_russian_callsign():
    """Generate a valid Russian callsign (European or Asiatic Russia)."""
    # Russian prefixes: R, RA, RW, RV, RN, RK, RZ, RC, RD, UA, UK
    # European Russia: R1-R7, RA1-RA7, UA1-UA7, etc.
    # Asiatic Russia: R0, R8, R9, RA0, RA8, RA9, UA9, UA0, etc.
    prefix = random.choice([
        'R', 'RA', 'RW', 'RV', 'RN', 'RK', 'RZ', 'UA', 'UK',
    ])

    # District digit — 0-9 covers EU Russia (1-7) and AS Russia (0, 8, 9)
    digit = str(random.randint(0, 9))

    # Suffix letters (typically 2-3)
    suffix_length = random.choices([2, 3], weights=[55, 45])[0]
    suffix = ''.join(random.choices(string.ascii_uppercase, k=suffix_length))

    return prefix + digit + suffix


def generate_international_callsign():
    """Generate a valid international callsign."""
    # Common international prefixes
    # Prefixes WITHOUT digits in them (need area digit added)
    prefixes_no_digit = [
        # Europe
        'G', 'M', 'GW', 'GI',  # UK (England, Scotland, Wales, Northern Ireland)
        'DL', 'DJ', 'DK',  # Germany
        'F',  # France
        'I',  # Italy
        'EA', 'EB',  # Spain
        'OH',  # Finland
        'SM',  # Sweden
        'LA',  # Norway
        'OZ',  # Denmark
        'PA', 'PE',  # Netherlands
        'ON',  # Belgium
        'HB',  # Switzerland
        'OE',  # Austria
        'SP',  # Poland
        'HA',  # Hungary
        'OK',  # Czech Republic
        # Asia/Pacific
        'JA', 'JE', 'JH', 'JR',  # Japan
        'HL',  # South Korea
        'BV',  # Taiwan
        'VK',  # Australia
        'ZL',  # New Zealand
        # South America
        'PY',  # Brazil
        'LU',  # Argentina
        'CE',  # Chile
        'YV',  # Venezuela
        # Caribbean
        'PJ',  # Sint Maarten/Curacao
        # Other
        'ZS',  # South Africa
    ]

    # Prefixes WITH digits (don't add area digit, just suffix)
    prefixes_with_digit = [
        'S5',  # Slovenia (S50-S59)
        'V4',  # St. Kitts (V40-V49)
        'J3',  # Grenada (J30-J39)
        '4X',  # Israel (4X0-4X9)
    ]

    if random.random() < 0.9:
        # Most common: prefix without digit
        prefix = random.choice(prefixes_no_digit)

        # Area digit (0-9)
        digit = str(random.randint(0, 9))

        # Suffix letters (typically 2-3 letters)
        suffix_length = random.choices([2, 3], weights=[60, 40])[0]
        suffix = ''.join(random.choices(string.ascii_uppercase, k=suffix_length))

        return prefix + digit + suffix
    else:
        # Less common: prefix with digit (S5, 4X, etc.)
        prefix = random.choice(prefixes_with_digit)

        # Suffix letters only (typically 2-3 letters)
        suffix_length = random.choices([2, 3], weights=[60, 40])[0]
        suffix = ''.join(random.choices(string.ascii_uppercase, k=suffix_length))

        return prefix + suffix


def generate_callsigns(us_count=100, canadian_mexican_count=50, intl_count=200, russian_count=0):
    """
    Generate a list of valid callsigns.

    Args:
        us_count: Number of US callsigns to generate
        canadian_mexican_count: Number of Canadian and Mexican callsigns (split evenly)
        intl_count: Number of international callsigns
        russian_count: Number of Russian callsigns to generate

    Returns:
        List of callsigns
    """
    callsigns = []

    # Generate US callsigns
    for _ in range(us_count):
        callsigns.append(generate_us_callsign())

    # Generate Canadian and Mexican callsigns (split evenly)
    canadian_count = canadian_mexican_count // 2
    mexican_count = canadian_mexican_count - canadian_count

    for _ in range(canadian_count):
        callsigns.append(generate_canadian_callsign())

    for _ in range(mexican_count):
        callsigns.append(generate_mexican_callsign())

    # Generate Russian callsigns
    for _ in range(russian_count):
        callsigns.append(generate_russian_callsign())

    # Generate international callsigns
    for _ in range(intl_count):
        callsigns.append(generate_international_callsign())

    # Shuffle the list
    random.shuffle(callsigns)

    return callsigns


def main():
    parser = argparse.ArgumentParser(
        description='Generate valid amateur radio callsigns for testing',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --us 100 --na 50 --intl 200
  %(prog)s -u 50 -n 25 -i 100
  %(prog)s --total 500  (generates mix with default ratios)
        """
    )

    parser.add_argument('-u', '--us', type=int, default=0,
                        help='Number of US callsigns to generate')
    parser.add_argument('-n', '--na', type=int, default=0,
                        help='Number of Canadian/Mexican callsigns to generate (split evenly)')
    parser.add_argument('-r', '--russian', type=int, default=0,
                        help='Number of Russian callsigns to generate')
    parser.add_argument('-i', '--intl', type=int, default=0,
                        help='Number of international callsigns to generate')
    parser.add_argument('-t', '--total', type=int,
                        help='Total number of callsigns (uses default ratios: 30%% US, 15%% NA, 55%% intl)')
    parser.add_argument('-o', '--output', type=str,
                        help='Output file (default: stdout)')
    parser.add_argument('--seed', type=int,
                        help='Random seed for reproducible results')

    args = parser.parse_args()

    # Set random seed if specified
    if args.seed is not None:
        random.seed(args.seed)

    # Calculate counts
    russian_count = args.russian
    if args.total:
        # Use default ratios (russian count is separate, subtracted from total first)
        remaining = args.total - russian_count
        us_count = int(remaining * 0.30)
        na_count = int(remaining * 0.15)
        intl_count = remaining - us_count - na_count
    else:
        us_count = args.us
        na_count = args.na
        intl_count = args.intl

    if us_count == 0 and na_count == 0 and intl_count == 0 and russian_count == 0:
        parser.print_help()
        return

    # Generate callsigns
    callsigns = generate_callsigns(us_count, na_count, intl_count, russian_count)

    # Output
    if args.output:
        with open(args.output, 'w') as f:
            for call in callsigns:
                f.write(call + '\n')
        print(f"Generated {len(callsigns)} callsigns to {args.output}")
    else:
        for call in callsigns:
            print(call)


if __name__ == "__main__":
    main()
