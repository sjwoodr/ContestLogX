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
    "sameContinent": {
      "CW": 2,
      "SSB": 1
    },
    "differentContinent": {
      "CW": 4,
      "SSB": 2
    }
  },
  "multipliers": {
    "type": "perBandAndMode",
    "description": "US states and DXCC countries",
    "categories": ["states", "dxcc"],
    "countOncePerBandAndMode": true
  },
  "finalScore": "SUM(points) * (stateMultipliers + dxccMultipliers)"
}
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
