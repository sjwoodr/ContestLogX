# ContestLogX Test Logs

This directory contains test log files (.clx format) used to validate changes to the ContestLogX application, particularly for ensuring that contest definition and scoring changes don't break existing functionality.

## Test Log Format

Each test log file is a JSON document containing:
- Contest metadata (name, mode, abbreviation)
- Station information (callsign, operator name)
- A collection of QSO records with known scoring and multiplier results

## Naming Convention

Test logs follow the naming pattern: `test_[contest_abbrev]_[mode]_log.clx`

Example: `test_naqp_cw_log.clx`

## Current Test Logs

### test_naqp_cw_log.clx
- **Contest**: North American QSO Party (CW)
- **Operator**: W5TEST (STEVE in FL)
- **QSOs**: 18 total contacts across 3 bands
  - 40m: 10 contacts (NC, MA, NY, AK, HI, PA, ON, XE, YV, PJ)
  - 15m: 5 contacts (NC dup, CA new, BC new, DX)
  - 10m: 3 contacts (MA dup, TX new, AZ new, DX)
- **Expected Score**: 
  - Total points: 18 (1 point each)
  - Total multipliers per band:
    - 40m: 8 (NC, MA, NY, AK, HI, PA, ON, XE)
    - 15m: 3 (CA, BC, new from 40m dupes don't count)
    - 10m: 2 (TX, AZ, new from earlier dupes don't count)
  - Total unique multipliers: 13 
  - Final score: 18 × 13 = 234
- **Purpose**: Validates:
  - Named multiplier tracking across multiple bands
  - Alaska/Hawaii state multiplier handling (not DXCC)
  - North American entity multiplier support (Mexico/XE)
  - Per-band multiplier counting (same mult on different bands counts separately)
  - Duplicate contact detection (same station/band)
  - Repeat contacts on different bands allowed
  - Mode-specific contest definitions
  - Station class and exchange field handling
  - Multi-band scoring scenarios

### test_naqp_ssb_log.clx
- **Contest**: North American QSO Party (SSB)
- **Operator**: W5TEST (STEVE in FL)
- **QSOs**: 18 total contacts across 3 bands
  - 80m: 10 contacts in LSB (NC, MA, NY, AK, HI, PA, ON, XE, YV, PJ)
  - 20m: 4 contacts in USB (NC dup, CA new, BC new, DX)
  - 15m: 4 contacts in USB (MA dup, TX new, AZ new, DX)
- **Expected Score**:
  - Total points: 18 (1 point each)
  - Total multipliers per band:
    - 80m: 8 (NC, MA, NY, AK, HI, PA, ON, XE)
    - 20m: 3 (CA, BC, new from 80m dupes don't count)
    - 15m: 2 (TX, AZ, new from earlier dupes don't count)
  - Total unique multipliers: 13
  - Final score: 18 × 13 = 234
- **Purpose**: Validates:
  - SSB mode operation (LSB on 80m, USB on 20m/15m)
  - 59 RST reports (standard for SSB)
  - Multi-band SSB scoring
  - Station class mode specificity (SO_ASSISTED_SSB)
  - Mode-based exchange handling

## Adding New Test Logs

When adding a new test log:

1. **Name it appropriately** following the naming convention
2. **Include comprehensive QSOs** that test:
   - Different multiplier types (states, provinces, DXCC, etc.)
   - Duplicate detection
   - Band multiplier handling
   - Edge cases specific to that contest
3. **Document expected results** including:
   - Total QSOs
   - Total points
   - Total multipliers (with breakdown by type)
   - Final claimed score
4. **Update this README** with the new test log info

## Usage

Test logs can be:
- Opened directly in ContestLogX via File → Open Log
- Used to validate scoring after contest definition or engine changes
- Compared against known good results to ensure regression testing

## Known Test Results

To add test results after validation, update this section with:
- Date last verified
- Version of ContestLogX verified with
- Any notes about test results
