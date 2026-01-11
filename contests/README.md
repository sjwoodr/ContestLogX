# ContestLogX Contest Definitions

This directory contains contest definition files in JSON format that define the rules, scoring, and UI requirements for each amateur radio contest supported by ContestLogX.

## Contest Definition Format

Each contest is defined in a JSON file with the following structure:

### Top-Level Sections

1. **contest** - Basic contest information
2. **frequencies** - Valid frequency ranges by band and mode
3. **stationClasses** - Station entry classes and operator information collection
4. **exchangeFields** - Information exchanged during QSOs
5. **qsoFields** - Fields stored for each QSO
6. **scoring** - Point and multiplier rules
7. **dupeChecking** - Duplicate contact rules
8. **logging** - Log file and Cabrillo format
9. **validation** - Contest rules and validation
10. **ui** - User interface configuration

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

### Station Classes Section

Defines entry classes for different operator categories. Some contests require operators to select a class, which may trigger additional input prompts for operator information:

```json
"stationClasses": {
  "enabled": true,
  "prompt": "What is your entry class?",
  "classes": [
    {
      "id": "MEMBER",
      "name": "Member",
      "description": "Send First Name and Member Number",
      "needsInput": true,
      "inputPrompts": {
        "name": "Enter your first name",
        "id": "Enter your member number"
      },
      "exchangeFieldMapping": {
        "name": "NAMEs",
        "id": "EXCHs"
      },
      "inputValidation": {
        "name": {
          "forceUppercase": true
        },
        "id": {
          "type": "numeric",
          "forceUppercase": false
        }
      },
      "exchangeSent": {
        "type": "customInput",
        "field": "memberExchange"
      }
    },
    {
      "id": "NON_MEMBER",
      "name": "Non-Member",
      "description": "Send First Name and State",
      "needsInput": true,
      "inputPrompts": {
        "name": "Enter your first name",
        "id": "Enter your state"
      },
      "exchangeFieldMapping": {
        "name": "NAMEs",
        "id": "EXCHs"
      },
      "inputValidation": {
        "name": {
          "forceUppercase": true
        },
        "id": {
          "type": "alphanumeric",
          "forceUppercase": true
        }
      },
      "exchangeSent": {
        "type": "customInput"
      }
    }
  ]
}
```

#### Station Classes Properties

- **enabled** - Whether station classes are used in this contest (boolean)
- **prompt** - Message displayed when asking operator to select a class
- **classes** - Array of available entry classes

#### Class Definition Properties

- **id** - Unique identifier for this class (alphanumeric, no spaces)
- **name** - Display name of the class
- **description** - Brief description of this entry class
- **needsInput** - Whether this class requires operator information input (boolean)
- **inputPrompts** - Object with prompts for operator input:
  - **name** - Prompt text for operator's name/first name
  - **id** - Prompt text for operator identifier (member number, state, country, etc.)
- **exchangeFieldMapping** - Maps input fields to exchange fields:
  - **name** - Maps input name to this exchange field (e.g., "NAMEs")
  - **id** - Maps input id to this exchange field (e.g., "EXCHs")
- **inputValidation** - Validation rules for operator input:
  - **name** - Validation for the name field
    - **forceUppercase** - Convert input to uppercase
  - **id** - Validation for the id field
    - **type** - Validation type: "numeric", "alphanumeric", or omit for free text
    - **defaultValue** - Pre-fill value (optional)
    - **forceUppercase** - Convert input to uppercase
- **exchangeSent** - How the exchange is formatted:
  - **type** - "customInput" (operator provides), "fixedValue" (constant), "state_province" (auto-filled), "serial" (auto-increment)
  - **value** - For fixedValue type, the constant string to send
  - **field** - For customInput type, optional field identifier

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

**Multiplier Categories:**
The `categories` field specifies what types of multipliers are counted:
- `namedMults`: Exchange-based multipliers from the `validation.namedMults` array (e.g., US states, Canadian provinces, serial numbers)
- `namedCallPrefixes`: Call sign prefix-based multipliers from the `validation.namedCallPrefixes` array. These are extracted from the callsign itself based on defined prefixes. For example, in the YBDX contest, YB0-YB9, YE0-YE9, YC0-YC9, YF0-YF9, YD0-YD9, YG0-YG9, 7A-7I, and 8A-8I are valid prefixes. When working YB2ARZ, the prefix "YB2" is extracted as a multiplier.
- `dxcc`: DXCC country entities (automatically looked up from callsign)
- `ituRegions`: ITU regions (automatically looked up from callsign)

Example with call prefixes (YBDX contest):
```json
"multipliers": {
  "type": "multsPerBand",
  "description": "YB call prefixes and DXCC countries",
  "categories": ["namedCallPrefixes", "dxcc"]
},
"validation": {
  "namedCallPrefixes": [
    "YB0", "YB1", "YB2", "YB3", "YB4", "YB5", "YB6", "YB7", "YB8", "YB9",
    "YE0", "YE1", "YE2", "YE3", "YE4", "YE5", "YE6", "YE7", "YE8", "YE9",
    "YC0", "YC1", "YC2", "YC3", "YC4", "YC5", "YC6", "YC7", "YC8", "YC9",
    "YF0", "YF1", "YF2", "YF3", "YF4", "YF5", "YF6", "YF7", "YF8", "YF9",
    "YD0", "YD1", "YD2", "YD3", "YD4", "YD5", "YD6", "YD7", "YD8", "YD9",
    "YG0", "YG1", "YG2", "YG3", "YG4", "YG5", "YG6", "YG7", "YG8", "YG9",
    "7A", "7B", "7C", "7D", "7E", "7F", "7G", "7H", "7I",
    "8A", "8B", "8C", "8D", "8E", "8F", "8G", "8H", "8I"
  ]
}
```

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
  "fieldNavigation": {
    "keys": "both",
    "description": "Keys used to navigate between QSO entry fields"
  },
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

#### Entry Field Navigation

The `fieldNavigation` object controls which keys advance focus to the next QSO entry field:

- **"space"** - Only the Space key advances to the next field. Tab behaves normally and may move focus outside the entry fields.
- **"tab"** - Only Tab (and Shift+Tab for backward) advance between fields. Space is inserted as text in fields.
- **"both"** - Both Space and Tab keys advance between fields (most efficient for rapid contest entry). Shift+Tab goes backward. Default if not specified.

Example configurations:
```json
"fieldNavigation": {
  "keys": "space"
}
```

When navigating fields, focus wraps around: the last field's Space/Tab moves back to the first field (Call).

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
