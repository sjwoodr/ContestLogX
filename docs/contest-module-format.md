# Contest Module Format

Complete reference for the ContestLogX contest definition JSON format.
Every contest is defined by a single `.json` file in the `contests/` directory.
Drop a new file there and it appears in the contest selection dialog immediately — no recompilation needed.

## Contents

1. [Top-Level Structure](#top-level-structure)
2. [contest](#contest)
3. [frequencies](#frequencies)
4. [stationClasses](#stationclasses)
5. [userPrompts](#userprompts)
6. [exchangeFields](#exchangefields)
7. [qsoFields](#qsofields)
8. [scoring](#scoring)
9. [dupeChecking](#dupechecking)
10. [logging](#logging)
11. [callHistory](#callhistory)
12. [validation](#validation)
13. [multAliases](#multaliases)
14. [ui](#ui)
15. [Minimal Example](#minimal-example)

---

## Top-Level Structure

A contest definition file contains these top-level keys:

```json
{
  "contest":        { },   // required — metadata, bands, modes
  "frequencies":    { },   // required — band/mode frequency ranges
  "stationClasses": { },   // required — entry classes (can be disabled)
  "userPrompts":    [ ],   // optional — setup questions collected at contest start
  "exchangeFields": { },   // required — sent/received exchange field definitions
  "qsoFields":      [ ],   // required — log table column definitions
  "scoring":        { },   // required — QSO points, multipliers, final score formula
  "dupeChecking":   { },   // required — duplicate detection scope
  "logging":        { },   // required — Cabrillo export settings
  "callHistory":    { },   // optional — fields to persist to call history
  "validation":     { },   // required — constraints and valid multiplier lists
  "ui":             { }    // required — log columns, entry fields, display options
}
```

---

## contest

Contest identity and metadata.

```json
"contest": {
  "name":               "North American QSO Party",
  "abbreviation":       "NAQP",
  "version":            "1.0.0",
  "sponsor":            "NCJWEB / CQ Magazine",
  "description":        "Optional longer description",
  "startDate":          "Second full weekend in January (CW)",
  "duration":           12,
  "offTimeGapMinutes":  30,
  "modes":              ["CW", "SSB", "RTTY"],
  "bands":              ["160m", "80m", "40m", "20m", "15m", "10m"],
  "url":                "https://www.ncjweb.com/naqp.html",
  "categories": {
    "mode":     ["CW", "SSB", "RTTY"],
    "stations": ["SINGLE_OP", "MULTI_OP"],
    "power":    ["QRP", "LOW_POWER", "HIGH_POWER"]
  }
}
```

| Field | Type | Notes |
|-------|------|-------|
| `name` | string | Full contest name shown in UI |
| `abbreviation` | string | Short code used in Cabrillo and filenames |
| `version` | string | Semver for tracking module revisions |
| `sponsor` | string | Sponsoring organization |
| `description` | string | Optional longer description |
| `startDate` | string | Human-readable date description |
| `duration` | number | Contest duration in hours; `0` = no limit (general logging) |
| `offTimeGapMinutes` | number | Minimum consecutive off-time in minutes; `0` = no off-time rule |
| `modes` | array | `"CW"`, `"SSB"`, `"RTTY"`, `"DIGITAL"`, `"FM"`, `"FT8"`, `"FT4"` |
| `bands` | array | HF: `"160m"`…`"6m"`; VHF/UHF: `"2m"`, `"70cm"`, etc. |
| `url` | string | Official contest rules URL |
| `categories` | object | Informational only; does not affect scoring logic |

---

## frequencies

An object keyed by band name. Each band object defines the overall frequency range and optional mode sub-ranges used for automatic mode detection when rig control is active. Frequencies are in **kHz** for HF bands.

```json
"frequencies": {
  "40m": {
    "start": 7000,
    "end":   7300,
    "cw":     {"start": 7000, "end": 7125},
    "phone":  {"start": 7125, "end": 7300},
    "digital":{"start": 7000, "end": 7125}
  },
  "6m": {
    "start": 50000,
    "end":   54000,
    "cw":  {"start": 50000, "end": 50100},
    "ssb": {"start": 50100, "end": 50300},
    "fm":  {"start": 52525, "end": 52625}
  }
}
```

Mode sub-range keys: `cw`, `phone`, `ssb` (alias for phone), `digital`, `fm`.
Bands with no mode sub-ranges (e.g., microwave) only need `start` and `end`.

---

## userPrompts

**Preferred approach** for collecting operator setup information (name, state, contest mode, power category, etc.). Supersedes the older per-class `needsInput` / `inputPrompts` fields inside `stationClasses`. Use `userPrompts` for all new contest modules.

An array of questions shown to the operator when starting a new log. Answers are stored in the `.clx` metadata and can be mapped to exchange fields. Works with `stationClasses.enabled: false` (as in MNQP and Winter Field Day) or alongside a simple station class list where the class itself has no input fields.

```json
"userPrompts": [
  {
    "id":           "contestMode",
    "question":     "Which contest weekend are you entering?",
    "type":         "select",
    "options": [
      {"value": "CW",  "label": "CW (Third full weekend in February)"},
      {"value": "SSB", "label": "Phone/SSB (First full weekend in March)"}
    ],
    "required":     true,
    "storeInMeta":  true,
    "restrictMode": true,
    "description":  "CW and Phone are separate contest weekends"
  },
  {
    "id":            "myExchange",
    "question":      "Enter your exchange (e.g., OH):",
    "type":          "text",
    "required":      true,
    "forceUppercase":true,
    "storeInMeta":   true,
    "validation":    "^[A-Z]{2,3}$",
    "description":   "Your sent exchange",
    "exchangeFieldMapping": {
      "myExchange": "EXCHs"
    }
  },
  {
    "id":       "objectiveMultipliers",
    "question": "Select Objective Multipliers you are claiming:",
    "type":     "checkboxes",
    "options": [
      {"value": "ALT_POWER",     "label": "Alternative Power (x1)", "points": 1},
      {"value": "AWAY_FROM_HOME","label": "Operate away from home (x3)", "points": 3}
    ],
    "required":    false,
    "storeInMeta": true
  }
]
```

| Field | Type | Notes |
|-------|------|-------|
| `id` | string | Unique identifier; also the key used in `exchangeFieldMapping` |
| `type` | string | `"text"`, `"select"`, or `"checkboxes"` |
| `options` | array | For `select` and `checkboxes`; each has `value` and `label`; checkboxes may also have `points` |
| `forceUppercase` | boolean | For `text` type — forces input to uppercase |
| `validation` | string | Regex applied to `text` type input |
| `storeInMeta` | boolean | If true, answer is stored in the `.clx` file metadata |
| `restrictMode` | boolean | For `select` type — if true, the selected value restricts the log to that mode only. Restored automatically when reopening a `.clx` file. |
| `exchangeFieldMapping` | object | Maps prompt `id` → exchange field name (e.g., `"myExchange": "EXCHs"`) |

---

## exchangeFields

Defines the sent and received exchange fields. These drive the QSO entry form and export templates.

```json
"exchangeFields": {
  "sent": [
    {
      "name":        "RST",
      "type":        "rst",
      "required":    true,
      "default":     "599",
      "description": "Signal report"
    },
    {
      "name":        "NAMEs",
      "type":        "string",
      "required":    true,
      "description": "Operator first name sent"
    }
  ],
  "received": [
    {
      "name":        "RST",
      "type":        "rst",
      "required":    false,
      "validation":  "^[1-5][1-9][1-9]?$",
      "description": "Signal report received"
    },
    {
      "name":        "EXCHr",
      "type":        "string",
      "required":    true,
      "description": "State/Province/Country received"
    }
  ]
}
```

### Field name conventions

Exchange field names use a 3–5 letter code with an `s` (sent) or `r` (received) suffix:

- `RSTs` / `RSTr` — RST signal report
- `NAMEs` / `NAMEr` — operator first name
- `EXCHs` / `EXCHr` — generic exchange (state, serial, etc.)
- `SNs` / `SNr` — serial number
- `GRIDs` / `GRIDr` — Maidenhead grid square
- `CATs` / `CATr` — category/class
- `LOCs` / `LOCr` — location identifier
- `EXCH` — single exchange field (no sent/rcvd variant)

### Field types

- `"rst"` — RST field with default 599 (or 59 for SSB)
- `"string"` — general text field

The `validation` property accepts a regex string applied to input.

---

## qsoFields

Defines the columns stored in the QSO record and displayed in the log table. DATE, TIME, CALL, FREQ, and MODE are standard; add contest-specific columns as needed.

```json
"qsoFields": [
  {"name":"Date",         "column":"DATE",    "type":"date",   "format":"yyyy-MM-dd", "required":true},
  {"name":"Time",         "column":"TIME",    "type":"time",   "format":"HH:mm:ss",   "required":true},
  {"name":"Callsign",     "column":"CALL",    "type":"string", "required":true, "uppercase":true},
  {"name":"Frequency",    "column":"FREQ",    "type":"number", "unit":"kHz",    "required":true},
  {"name":"Mode",         "column":"MODE",    "type":"enum",   "values":["CW","SSB"], "required":true},
  {"name":"RST Sent",     "column":"RST_SENT","type":"string", "required":true, "default":"599"},
  {"name":"RST Received", "column":"RST_RCV", "type":"string", "required":true},
  {"name":"QTH Sent",     "column":"EXCHs",   "type":"string", "required":true},
  {"name":"QTH Received", "column":"EXCHr",   "type":"string", "required":true},
  {"name":"Points",       "column":"POINTS",  "type":"number", "calculated":true},
  {"name":"Multiplier",   "column":"M",       "type":"string", "calculated":true}
]
```

- Set `"calculated": true` on fields computed by the scoring engine (Points, Multiplier).
- Set `"default"` to pre-fill the entry field.
- The `column` value is the key used in `ui.logColumns`.

---

## scoring

### QSO Points

Points are defined by the geographic relationship between stations. Each rule is an object keyed by mode.

#### Geographic relationship rules

```json
"points": {
  "sameDxccEntity":      {"CW": 1, "SSB": 1},
  "differentDxccEntity": {"CW": 3, "SSB": 2},
  "sameContinent":       {"CW": 2, "SSB": 1},
  "differentContinent":  {"CW": 5, "SSB": 3},
  "namedCallPrefixes":   {"SSB": 10},
  "euCountry":           {"CW": 10}
}
// Aliases: "sameCountry" = sameDxccEntity, "differentCountry" = differentDxccEntity
```

#### Flat per-QSO points (all modes equal)

```json
"points": {"perQso": 1}
```

#### Points by mode category (e.g., Winter Field Day)

```json
"points": {
  "phone":   1,
  "cw":      2,
  "digital": 2
}
```

#### Points by band and contest month (ARRL VHF)

```json
"points": {
  "byBand": {
    "january":   {"6m": 1, "2m": 1, "70cm": 2, "23cm": 4},
    "june":      {"6m": 1, "2m": 1, "70cm": 2, "23cm": 3},
    "september": {"6m": 1, "2m": 1, "70cm": 2, "23cm": 3}
  }
}
```

The contest month is selected by the operator via a `userPrompts` entry with `id: "contestMonth"`.

### precedence

An ordered array of point rule names. The engine evaluates each rule in order and applies the **first match**. Any rule defined in `points` but absent from `precedence` is never applied (a warning is logged).

```json
"precedence": [
  "sameDxccEntity",
  "sameContinent",
  "differentContinent"
]
```

### multipliers

```json
"multipliers": {
  "type":                    "multsPerBand",
  "description":             "States and DXCC countries per band",
  "categories":              ["namedMults", "dxcc"],
  "alaskaAndHawaiiCountDxcc":false,
  "usAndCanadaCountDxcc":    false,
  "countOncePerBand":        true,
  "stationClassMultipliers": {
    "W_VE": ["dxcc"],
    "DX":   ["namedMults"]
  }
}
```

**multiplier types:**

| type | Description |
|------|-------------|
| `multsOnce` | Each multiplier counted once for the whole contest |
| `multsPerBand` | Each multiplier counted once per band |
| `multsPerMode` | Each multiplier counted once per mode |
| `multsPerBandAndMode` | Each multiplier counted once per band/mode combination |
| `objectiveMultipliers` | Score multiplied by user-selected objectives (Winter Field Day) |

**multiplier categories:**

| category | Description |
|----------|-------------|
| `namedMults` | Values from `validation.namedMults` list (states, provinces, counties, etc.) |
| `dxcc` | DXCC entities from the DXCC database |
| `namedCallPrefixes` | Call sign prefixes from `validation.namedCallPrefixes` (e.g., YB DX) |
| `gridSquares` | Maidenhead grid squares (ARRL VHF) |
| `objectiveMultipliers` | User-selected objective checkboxes (Winter Field Day) |

### finalScore

A formula string controlling how the final score is computed:

```json
// Most contests:
"finalScore": "SUM(points) * SUM(multipliers)"
"finalScore": "SUM(points) * (namedMults + dxccMultipliers)"
"finalScore": "SUM(points) * (namedCallPrefixes + dxccMultipliers)"
"finalScore": "SUM(points) * SUM(gridSquareMultipliers)"

// No multipliers:
"finalScore": "SUM(points)"

// Objective multipliers (Winter Field Day):
"finalScore": "(SUM(points)) * (objectiveMultiplierCount + 1)"
```

---

## dupeChecking

```json
"dupeChecking": {
  "type":        "perBand",
  "description": "Station may be worked once per band"
}
```

| type | Description |
|------|-------------|
| `overall` | Once per contest, regardless of band or mode |
| `perBand` | Once per band (most HF contests) |
| `perMode` | Once per mode |
| `perBandAndMode` | Once per band/mode combination (e.g., CW and SSB on 40m are separate) |
| `perBandAndGridSquare` | Once per band per grid square (ARRL VHF — rovers may be worked from multiple grids) |

---

## logging

Controls Cabrillo export. The `qsoTemplate` uses `{token}` placeholders filled at export time.

```json
"logging": {
  "requiredFields": ["date","time","frequency","mode","callsign","rstSent","rstReceived","exchSent","exchReceived"],
  "cabrillo": {
    "version":     "3.0",
    "contest":     "NAQP-CW",
    "contestMapping": {
      "CW":   "NAQP-CW",
      "SSB":  "NAQP-SSB",
      "RTTY": "NAQP-RTTY"
    },
    "qsoTemplate": "QSO: {freq} {mode} {date} {time} {mycall} {name_sent} {exch_sent} {call} {name_rcvd} {exch_rcvd}",
    "requiredHeaders": ["CONTEST","CALLSIGN","CATEGORY-OPERATOR","CLAIMED-SCORE","CREATED-BY","NAME","EMAIL"]
  }
}
```

Use `contestMapping` when the Cabrillo contest name varies by mode (e.g., NAQP-CW vs NAQP-SSB). The `qsoTemplate` tokens reference field names from `qsoFields`.

---

## callHistory

Specifies which received exchange fields should be saved to the persistent call history database for future autofill.

```json
"callHistory": {
  "fieldsToSave": ["NAMEr", "EXCHr"],
  "description":  "Save operator name and QTH for future lookups"
}
```

Set `fieldsToSave` to `[]` if no fields should be persisted (e.g., serial-number-only contests).

---

## validation

```json
"validation": {
  "minimumQSOs":           10,
  "maxOperatingTime":      12,
  "offTimesRequired":      true,
  "minimumOffTimeMinutes": 30,

  "namedMults": ["AL","AK","AZ","CA", ...],

  "namedCallPrefixes": ["YB0","YB1","YC0", ...],

  "gridSquareFormat": "^[A-R]{2}[0-9]{2}[a-x]{2}$",

  "exchangeValidation": {
    "type":               "namedMultOrSerial",
    "description":        "State/Province or serial number",
    "serialNumberFormat": "^[0-9]{1,4}$",
    "logic":              "Accept if matches namedMult OR serialNumberFormat"
  },

  "receivedExchangeFilter": {
    "promptId": "stationType",
    "rules": {
      "WVE": "inStateMults",
      "DX":  "inStateMults"
    }
  },
  "inStateMults": ["HEN","WRI","RAM", ...]
}
```

### exchangeValidation types

| type | Description |
|------|-------------|
| `nameAndMultiplier` | Name field is free text; QTH field must match `namedMults` |
| `namedMultOrSerial` | Accepts a named mult OR a serial number (format in `serialNumberFormat`) |
| `namedMultOrPower` | Accepts a named mult OR a power value (format in `powerFormat`) |
| `maidenheadGrid` | Validates against `gridSquareFormat` regex |
| `serial` | Numeric serial number only |
| `freeForm` | No validation — user is responsible for correct entry |

### namedMultAliases (unconditional 1:1 remap)

A simple key→value table applied to **every** received exchange before mult lookup. Use it for input forgiveness or for codes that are scoring-equivalent.

```json
"validation": {
  "namedMultAliases": {
    "5":  "05",
    "DC": "MD"
  }
}
```

Unlike `multAliases` (the conditional system documented below), this remap is unconditional — it applies regardless of operator class. Common uses:

- The operator types a short form that doesn't appear in the `namedMults` list (`"5"` → `"05"`).
- Two codes are administratively distinct but scoring-equivalent (`"DC"` → `"MD"` for ARRL contests where DC ARES counts as MD).

---

## multAliases

Conditionally rewrite a received exchange value to a different multiplier based on the operator's `userPrompts` answer. Used when the same on-air exchange means different things to different participant classes.

Two examples that drove the design:

- **FQP (Florida QSO Party)** — Florida operators send their county code (e.g., `ALA`) and receive county codes back from in-state contacts. But for a Florida operator, every Florida county counts as a single `FL` mult — they're not collecting their own counties. A `multAliases` rule maps "any value in `inStateMults`" → `FL`, *only when the operator is a FL station*.

- **7QP (7th Call Area QSO Party)** — 7th-area stations work everyone, including each other. Two 7th-area stations exchange 5-letter `<state><county>` codes (e.g., `WYALB`, `ORDES`). For a 7th-area operator, those codes count toward the **state** multiplier, not the county. A `multAliases` rule extracts the first 2 characters as the mult value, *only when the operator is in one of the eight 7th-area states*.

### JSON shape

`multAliases` lives at the **top level** of the contest definition (sibling to `scoring`, `dupeChecking`, etc.) and contains an array of rule objects:

```json
"multAliases": [
  {
    "promptId":      "stationType",
    "promptValueIn": ["AZ","ID","MT","NV","OR","UT","WA","WY"],
    "sourceList":    "inStateMults",
    "mapByPrefix":   2,
    "comment":       "7QP: 7th-area ops see WYALB → WY, ORDES → OR, etc."
  },
  {
    "promptId":     "stationType",
    "promptValue":  "FL",
    "sourceList":   "inStateMults",
    "mapsTo":       "FL",
    "comment":      "FQP: Florida ops collapse all FL counties to a single FL mult."
  }
]
```

### Trigger fields

The rule fires when the operator's answer to `promptId` (set during contest startup via `userPrompts`) satisfies the trigger.

| Field | Purpose |
|-------|---------|
| `promptId` | Required. The `userPrompts` id whose stored answer is checked. |
| `promptValue` | Single-value trigger — fires when the operator's answer equals this string. |
| `promptValueIn` | Array trigger — fires when the operator's answer is in this list. Overrides `promptValue` if non-empty. |

### Mapping fields

When a rule fires, the engine inspects the rawMult value (the received exchange string) and applies a mapping if it's in the rule's `sourceList`.

| Field | Effect |
|-------|--------|
| `sourceList` | Required. Either `"inStateMults"` (the same list referenced by `validation.inStateMults`) or `"namedMults"` (`validation.namedMults`). The rule's mapping only applies if rawMult is in this set. |
| `mapsTo` | Static replacement: rawMult is replaced with this string in the engine's mult lookup. |
| `mapByPrefix` | Prefix extraction: take `rawMult.left(N)` as the mult value. Use for state-prefixed codes (e.g., 7QP's 5-letter `<state><county>` → 2-char state with `mapByPrefix: 2`). Wins over `mapsTo` when both are present. |

### How it fires

When a QSO is logged:

1. The received exchange value is extracted from the `EXCHr` field.
2. If any `multAliases` rule's trigger matches the current operator (`promptId` answer matches `promptValue` or appears in `promptValueIn`), AND rawMult is in that rule's `sourceList`, the alias activates.
3. The aliased value (`mapsTo` or `rawMult.left(mapByPrefix)`) replaces rawMult as the credited multiplier.
4. If no rule matches, rawMult is used unchanged.

The check happens after `validation.namedMultAliases` (the unconditional 1:1 remap), so a code can be canonicalized first and then aliased per-operator.

### Multiplier panel display

When a `multAliases` rule is active for the current operator, the values in its `sourceList` are **hidden from the multiplier panel display**. They would just clutter the panel — the operator is earning credit for the aliased *target*, not the individual source values. The remaining `namedMults` (states, provinces, DX, etc., that don't appear in the source list) are shown normally.

For example, a 7QP operator in Wyoming sees ~65 destination mults in the multiplier panel (50 states + DC + 13 provinces + DX), while the 259 5-letter county codes are hidden because they all alias to state codes via `mapByPrefix: 2`.

---

## ui

```json
"ui": {
  "showMultiplierPanel": true,
  "logColumns":   ["DATE","TIME","CALL","FREQ","MODE","RSTs","RSTr","NAMEs","NAMEr","EXCHs","EXCHr","POINTS","M"],
  "entryFields":  ["CALL","NAMEr","EXCHr"],
  "fieldNavigation": {
    "keys": "tab"
  },
  "bandMap": {
    "enabled": true,
    "bands":   ["160m","80m","40m","20m","15m","10m"]
  },
  "multiplierDisplay": {
    "showStates":     true,
    "showProvinces":  true,
    "showDXCC":       true,
    "showCounties":   false,
    "showYBPrefixes": false
  }
}
```

| Field | Notes |
|-------|-------|
| `showMultiplierPanel` | Shows the multiplier checkbox panel (requires `validation.namedMults`) |
| `logColumns` | Ordered list of column keys shown in the QSO log table; must match `qsoFields[].column` values |
| `entryFields` | Fields shown in the QSO entry row (Tab navigates between them). `CALL` is always first. |
| `fieldNavigation.keys` | `"tab"` — Tab only; `"both"` — Tab and Space both advance fields |
| `bandMap.enabled` | Shows the band selector buttons |
| `multiplierDisplay` | Controls which multiplier groups appear in the multiplier panel |

---

## Minimal Example

A simple club sprint: 4-hour CW-only, 40m/20m, 1 point per QSO, unique callsigns as multipliers.
Save as `contests/my_club_sprint.json` and restart ContestLogX.

```json
{
  "contest": {
    "name":              "My Club Sprint",
    "abbreviation":      "MCS",
    "version":           "1.0.0",
    "sponsor":           "My Amateur Radio Club",
    "startDate":         "First Saturday of March",
    "duration":          4,
    "offTimeGapMinutes": 0,
    "modes": ["CW"],
    "bands": ["40m", "20m"],
    "url":   ""
  },
  "frequencies": {
    "40m": {"start":7000,  "end":7300,  "cw":{"start":7000,  "end":7125}},
    "20m": {"start":14000, "end":14350, "cw":{"start":14000, "end":14150}}
  },
  "stationClasses": {
    "enabled": true,
    "prompt":  "Enter your name and state",
    "classes": [
      {
        "id":   "SO",
        "name": "Single Operator",
        "needsInput": true,
        "inputPrompts": {"name":"Your first name","id":"Your state/province"},
        "exchangeFieldMapping": {"name":"NAMEs","id":"EXCHs"},
        "inputValidation": {"name":{"forceUppercase":true},"id":{"forceUppercase":true}},
        "exchangeSent": {"type":"customInput"}
      }
    ]
  },
  "exchangeFields": {
    "sent": [
      {"name":"RST",   "type":"rst",    "required":true},
      {"name":"NAMEs", "type":"string", "required":true},
      {"name":"EXCHs", "type":"string", "required":true}
    ],
    "received": [
      {"name":"RST",   "type":"rst",    "required":false},
      {"name":"NAMEr", "type":"string", "required":true},
      {"name":"EXCHr", "type":"string", "required":true}
    ]
  },
  "qsoFields": [
    {"name":"Date",     "column":"DATE",   "type":"date",   "format":"yyyy-MM-dd", "required":true},
    {"name":"Time",     "column":"TIME",   "type":"time",   "format":"HH:mm:ss",   "required":true},
    {"name":"Callsign", "column":"CALL",   "type":"string", "required":true, "uppercase":true},
    {"name":"Frequency","column":"FREQ",   "type":"number", "unit":"kHz",    "required":true},
    {"name":"Mode",     "column":"MODE",   "type":"enum",   "values":["CW"], "required":true},
    {"name":"Name Sent","column":"NAMEs",  "type":"string", "required":true},
    {"name":"Name Rcvd","column":"NAMEr",  "type":"string", "required":true},
    {"name":"QTH Sent", "column":"EXCHs",  "type":"string", "required":true},
    {"name":"QTH Rcvd", "column":"EXCHr",  "type":"string", "required":true},
    {"name":"Points",   "column":"POINTS", "type":"number", "calculated":true}
  ],
  "scoring": {
    "points": {"perQso": 1},
    "multipliers": {
      "type":       "multsOnce",
      "categories": ["namedMults"]
    },
    "finalScore": "SUM(points) * SUM(multipliers)"
  },
  "dupeChecking": {"type":"perBand"},
  "logging": {
    "requiredFields": ["date","time","frequency","mode","callsign","rstSent","rstReceived","exchSent","exchReceived"],
    "cabrillo": {
      "version":     "3.0",
      "contest":     "MCS",
      "qsoTemplate": "QSO: {freq} {mode} {date} {time} {mycall} {rst_sent} {name_sent} {exch_sent} {call} {rst_rcvd} {name_rcvd} {exch_rcvd}"
    }
  },
  "callHistory": {"fieldsToSave":["NAMEr","EXCHr"]},
  "validation": {
    "minimumQSOs":      1,
    "maxOperatingTime": 4,
    "offTimesRequired": false,
    "namedMults": [
      "AL","AK","AZ","AR","CA","CO","CT","DE","FL","GA","HI","ID","IL","IN","IA",
      "KS","KY","LA","ME","MD","MA","MI","MN","MS","MO","MT","NE","NV","NH","NJ",
      "NM","NY","NC","ND","OH","OK","OR","PA","RI","SC","SD","TN","TX","UT","VT",
      "VA","WA","WV","WI","WY","DC",
      "AB","BC","MB","NB","NL","NS","NU","NT","ON","PE","QC","SK","YT"
    ],
    "exchangeValidation": {"type":"nameAndMultiplier"}
  },
  "ui": {
    "logColumns":  ["DATE","TIME","CALL","FREQ","MODE","NAMEs","NAMEr","EXCHs","EXCHr","POINTS"],
    "entryFields": ["CALL","NAMEr","EXCHr"],
    "fieldNavigation": {"keys":"tab"},
    "bandMap": {"enabled":true,"bands":["40m","20m"]}
  }
}
```
