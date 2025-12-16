# ContestLogX Contest Definitions

This directory contains contest definition files in JSON format that define the rules, scoring, and UI requirements for each amateur radio contest supported by ContestLogX.

## Contest Definition Format

Each contest is defined in a JSON file with the following structure:

### Top-Level Sections

1. **contest** - Basic contest information
2. **frequencies** - Valid frequency ranges by band and mode
3. **exchangeFields** - Information exchanged during QSOs
4. **qsoFields** - Fields stored for each QSO
5. **scoring** - Point and multiplier rules
6. **dupeChecking** - Duplicate contact rules
7. **logging** - Log file and Cabrillo format
8. **validation** - Contest rules and validation
9. **ui** - User interface configuration

### Contest Section

```json
"contest": {
  "name": "Full Contest Name",
  "abbreviation": "ABBREV",
  "sponsor": "Sponsoring Organization",
  "startDate": "Description of start date",
  "duration": 48,
  "modes": ["CW", "SSB", "DIGITAL"],
  "bands": ["10m", "15m", "20m"],
  "url": "https://contest-rules-url.com",
  "categories": {
    "power": ["HIGH", "LOW", "QRP"],
    "modes": ["CW", "SSB", "MIXED"],
    "stations": ["SINGLE_OP", "MULTI_OP"]
  }
}
```

### Frequencies Section

Defines valid operating frequencies for each band:

```json
"frequencies": {
  "10m": {
    "start": 28000,
    "end": 29700,
    "cw": {"start": 28000, "end": 28300},
    "digital": {"start": 28000, "end": 28300},
    "phone": {"start": 28300, "end": 29700}
  }
}
```

### Exchange Fields Section

Defines what information is sent and received:

```json
"exchangeFields": {
  "sent": [
    {
      "name": "RST",
      "type": "string",
      "required": true,
      "default": "599",
      "description": "Signal report"
    }
  ],
  "received": [
    {
      "name": "RST",
      "type": "string",
      "required": true,
      "validation": "^[1-5][1-9][1-9]$",
      "description": "Signal report"
    }
  ]
}
```

### QSO Fields Section

Defines all fields stored for each contact:

```json
"qsoFields": [
  {
    "name": "Callsign",
    "column": "CALL",
    "type": "string",
    "required": true,
    "uppercase": true
  },
  {
    "name": "Mode",
    "column": "MODE",
    "type": "enum",
    "values": ["CW", "SSB", "DIGITAL"],
    "required": true
  }
]
```

#### Field Types

- **string** - Text field
- **number** - Numeric value
- **date** - Date field
- **time** - Time field
- **enum** - Select from predefined values
- **boolean** - True/false

#### Field Properties

- **name** - Display name
- **column** - Column header in log
- **type** - Data type
- **required** - Must be filled
- **default** - Default value
- **uppercase** - Convert to uppercase
- **validation** - Regex pattern
- **calculated** - Auto-calculated
- **format** - Date/time format
- **unit** - Unit of measurement

### Scoring Section

Defines how points and multipliers are calculated:

```json
"scoring": {
  "points": {
    "sameDxccEntity": {
      "CW": 1,
      "SSB": 1
    },
    "differentDxccEntity": {
      "CW": 3,
      "SSB": 2
    },
    "sameContinent": {
      "CW": 2,
      "SSB": 1
    },
    "differentContinent": {
      "CW": 4,
      "SSB": 2
    }
  },
  "precedence": [
    "sameDxccEntity",
    "differentDxccEntity",
    "sameContinent",
    "differentContinent"
  ],
  "multipliers": {
    "type": "multsPerBandAndMode",
    "description": "US states and DXCC countries",
    "categories": ["states", "dxcc"],
    "alaskaAndHawaiiAre": "states"
  },
  "finalScore": "SUM(points) * multipliers"
}
```

**US/Canada DXCC Counting:**
In some contests, US states and Canadian provinces count as BOTH state/province multipliers AND DXCC entity multipliers. In other contests, they count ONLY as state/province multipliers.

- `"usAndCanadaCountDxcc": true` - US/Canada stations count as BOTH their state/province AND their DXCC entity (K for USA, VE for Canada). Example: Working W4ABC in NC counts as both NC state mult and K DXCC mult. (Default if not specified)
- `"usAndCanadaCountDxcc": false` - US/Canada stations count ONLY as their state/province. They do NOT contribute to DXCC multipliers. Example: Working W4ABC in NC only counts as NC state mult.

**Alaska and Hawaii Special Handling:**
Alaska (AK) and Hawaii (HI) are both US states AND separate DXCC entities (KL7 and KH6). Different contests treat them differently:
- `"alaskaAndHawaiiAre": "states"` - Count only as state multipliers (e.g., ARRL 10M, Sweepstakes)
- `"alaskaAndHawaiiAre": "dxcc"` - Count only as DXCC multipliers (use KL7/KH6)
- `"alaskaAndHawaiiAre": "both"` - Count as BOTH state AND DXCC multipliers (default if not specified)
- `"alaskaAndHawaiiAre": "none"` - AK and HI are NOT counted as multipliers at all

The contest engine automatically detects Alaska/Hawaii stations by DXCC lookup and applies the appropriate multiplier handling.

**Multiplier Types:**
The `type` field determines how multipliers are counted:
- `multsOnce`: Each multiplier counts only once (e.g., work OH once for entire contest)
- `multsPerBand`: Each multiplier counts once per band (e.g., work OH on 20m and 40m = 2 mults)
- `multsPerMode`: Each multiplier counts once per mode (e.g., work OH on CW and SSB = 2 mults)
- `multsPerBandAndMode`: Each multiplier counts once per band/mode combination (e.g., work OH on 20m CW, 20m SSB, 40m CW = 3 mults)

**Scoring Rules:**
- `sameDxccEntity`: Points for contacts within same DXCC entity (e.g., W1AW to N9OH)
- `differentDxccEntity`: Points for contacts with different DXCC entity
- `sameCountry`: Backward compatibility alias for `sameDxccEntity`
- `differentCountry`: Backward compatibility alias for `differentDxccEntity`
- `sameContinent`: Points for same continent contacts
- `differentContinent`: Points for different continent contacts

**Precedence Array:**
The optional `precedence` array defines the order in which scoring rules are evaluated. The first matching rule is used. If not specified, defaults to:
```json
["sameDxccEntity", "sameCountry", "differentDxccEntity", "differentCountry", "sameContinent", "differentContinent"]
```

**Important:** Only rules listed in the `precedence` array will be checked. If you define a scoring rule in `points` but don't include it in `precedence`, it will be **ignored** and a warning will be logged.

Example: To prioritize continent scoring over DXCC:
```json
"precedence": ["sameContinent", "differentContinent", "sameDxccEntity", "differentDxccEntity"]
```

Example: DXCC-only scoring (ignores continent rules even if defined):
```json
"precedence": ["sameDxccEntity", "differentDxccEntity"]
```

### Dupe Checking Section

Defines when a contact is considered a duplicate:

```json
"dupeChecking": {
  "type": "perBandAndMode",
  "description": "Station may be worked once per band per mode"
}
```

#### Dupe Check Types

- **once** - Only once in entire contest
- **perBand** - Once per band
- **perMode** - Once per mode
- **perBandAndMode** - Once per band/mode combination
- **custom** - Custom logic required

### Logging Section

Defines log file formats and Cabrillo export:

```json
"logging": {
  "requiredFields": ["date", "time", "frequency", "mode", "callsign"],
  "cabrillo": {
    "version": "3.0",
    "contest": "ARRL-10",
    "qsoTemplate": "QSO: {freq} {mode} {date} {time} {mycall} {rst_sent} {exch_sent} {call} {rst_rcvd} {exch_rcvd}"
  }
}
```

### Validation Section

Contest rules and validation requirements:

```json
"validation": {
  "minimumQSOs": 1,
  "maxOperatingTime": 48,
  "offTimesRequired": false,
  "exchangeValidation": {
    "states": ["CA", "OR", "WA", ...],
    "provinces": ["AB", "BC", "ON", ...],
    "serialNumberFormat": "^[0-9]{1,4}$"
  }
}
```

### UI Section

User interface configuration:

```json
"ui": {
  "logColumns": ["SEQ", "DATE", "TIME", "CALL", "MODE", "RST_SENT", "RST_RCV"],
  "entryFields": ["CALL", "RST_RCV", "EXC"],
  "bandMap": {
    "enabled": true,
    "bands": ["10m", "15m", "20m"]
  },
  "multiplierDisplay": {
    "showStates": true,
    "showDXCC": true
  }
}
```

## Creating a New Contest

1. Create a new JSON file in this directory: `contest_name.json`
2. Define all required sections based on contest rules
3. Test the contest definition in ContestLogX
4. Validate Cabrillo export matches contest requirements

## Examples

- **arrl_10m.json** - ARRL 10 Meter Contest
  - Single band (10m)
  - CW, SSB, and Digital modes
  - State/Province/Serial number exchange
  - Points based on same/different continent
  - Multipliers per band and mode

## Notes

- Frequencies are in kHz
- Dates use ISO 8601 format (yyyy-MM-dd)
- Times use 24-hour format (HH:mm:ss)
- All callsigns are automatically converted to uppercase
- Serial numbers are automatically incremented

## Future Enhancements

- Support for multi-transmitter operation
- Custom scoring formulas
- Real-time multiplier tracking
- Band-specific exchange requirements
- Assisted/Non-assisted categories
