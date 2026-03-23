# Data Model: Online Score Publishing

## Extended Entities

### StationInfo (existing — extended)
| Field       | Type    | New? | Notes                                      |
|-------------|---------|------|--------------------------------------------|
| callsign    | QString | no   | Operator callsign                          |
| operator    | QString | no   | Operator name                              |
| grid        | QString | no   | Maidenhead grid square (4 or 6 char)       |
| state       | QString | no   | State/province abbreviation                |
| county      | QString | no   | County (for QSO parties)                   |
| cqZone      | int     | **yes** | CQ zone (1-40), auto-populated from DXCC |
| ituZone     | int     | **yes** | ITU zone (1-90), auto-populated from DXCC|
| arrlSection | QString | **yes** | ARRL/RAC section (optional)              |
| rig         | QString | no   | Equipment description                      |
| antenna     | QString | no   | Antenna description                        |
| power       | int     | no   | Power in watts                             |

**JSON serialization**: cqZone, ituZone added to `location` object. arrlSection added to `location` object.

### Settings — onlineScoring (new block)
| Field           | Type    | Default                                     |
|-----------------|---------|---------------------------------------------|
| callsign        | QString | "" (empty = not configured)                 |
| password        | QString | "" (XOR + Base64 encoded)                   |
| intervalMinutes | int     | 5                                           |
| perQso          | bool    | false                                       |
| club            | QString | "" (from existing Cabrillo club setting)    |

**Storage location**: `m_settings["onlineScoring"]` in ContestLogX.json

### Contest Definition — contestOnlineScore (new block)
| Field             | Type    | Required | Notes                                  |
|-------------------|---------|----------|----------------------------------------|
| contestId         | QString | yes      | Server contest ID (e.g., "NAQP-CW")   |
| contestIdMapping  | Object  | no       | Maps userPrompt value → contest ID (for mode-dependent contests) |
| mult1Name         | QString | no       | Display name for first multiplier      |
| mult1Attribute    | QString | no       | Server attribute: zone/country/state/gridsquare/wpxprefix/prefix/hq |
| mult2Name         | QString | no       | Display name for second multiplier     |
| mult2Attribute    | QString | no       | Server attribute for second multiplier |

**Example (single ID)**:
```json
"contestOnlineScore": {
    "contestId": "CW-Ops",
    "mult1Name": "Unique Calls",
    "mult1Attribute": "state"
}
```

**Example (mode-dependent ID)**:
```json
"contestOnlineScore": {
    "contestId": "NAQP-CW",
    "contestIdMapping": {
        "contestMode": {
            "CW": "NAQP-CW",
            "SSB": "NAQP-SSB",
            "RTTY": "NAQP-RTTY"
        }
    },
    "mult1Name": "States/Provinces/NA",
    "mult1Attribute": "state"
}
```

## New Entities

### ScorePostData (transient — built per posting cycle)
| Field         | Type                          | Source                              |
|---------------|-------------------------------|-------------------------------------|
| contestId     | QString                       | contestOnlineScore.contestId (or mapping) |
| callsign      | QString                       | Session callsign                    |
| ops           | QString                       | Session callsign (single-op) or operator list |
| softName      | QString                       | "ContestLogX"                       |
| softVersion   | QString                       | APP_VERSION                         |
| club          | QString                       | Settings Cabrillo club              |
| classAttrs    | ClassAttributes               | Derived from userPrompts            |
| qth           | QthData                       | From session station info + DXCC    |
| breakdown     | QList\<BandModeBreakdown\>    | From contest engine                 |
| totalScore    | int                           | From ContestScore.contestScore      |
| timestamp     | QDateTime                     | QDateTime::currentDateTimeUtc()     |

### ClassAttributes (value object)
| Field       | Type    | Values                                         | Default         |
|-------------|---------|------------------------------------------------|-----------------|
| power       | QString | HIGH, LOW, QRP                                 | HIGH            |
| assisted    | QString | NON-ASSISTED, ASSISTED                         | NON-ASSISTED    |
| transmitter | QString | ONE, TWO, UNLIMITED                            | ONE             |
| ops         | QString | SINGLE-OP, MULTI-OP                            | SINGLE-OP       |
| bands       | QString | ALL, 160M, 80M, 40M, 20M, 15M, 10M, etc.     | ALL             |
| mode        | QString | MIXED, CW, PH, RTTY, PSK, FT8, FT4, DIGI     | MIXED           |
| overlay     | QString | N/A, TB-WIRES, ROOKIE, CLASSIC, WIRE-ONLY     | N/A             |

### QthData (value object)
| Field        | Type    | Source                           |
|--------------|---------|----------------------------------|
| dxccCountry  | QString | DXCC primary prefix from callsign|
| cqZone       | int     | StationInfo.cqZone               |
| iaruZone     | int     | StationInfo.ituZone              |
| arrlSection  | QString | StationInfo.arrlSection          |
| stPrvOth     | QString | StationInfo.state                |
| grid         | QString | StationInfo.grid (4 or 6 char)   |

### BandModeBreakdown (value object)
| Field         | Type    | Notes                                   |
|---------------|---------|-----------------------------------------|
| band          | QString | "20", "40", "80", "160", "15", "10", or "total" |
| mode          | QString | "CW", "PH", "RY", "DG", or "ALL"       |
| qsoCount      | int     | QSO count for this band/mode            |
| points        | int     | Points for this band/mode               |
| mult1Count    | int     | First multiplier count (optional)       |
| mult2Count    | int     | Second multiplier count (optional)      |
| mult1Type     | QString | "zone", "country", "state", etc.        |
| mult2Type     | QString | Second multiplier type (optional)       |

## State Machine: Online Scoring Session

```
DISABLED → (user enables via Contest menu) → VALIDATING
VALIDATING → (all fields present) → ENABLED
VALIDATING → (fields missing) → DISABLED (show warning dialog)
ENABLED → (immediate first post) → POSTING
POSTING → (success) → IDLE (show last post time)
POSTING → (failure) → ERROR (show error, schedule retry)
ERROR → (next interval) → POSTING
ERROR → (3 consecutive auth failures) → DISABLED (show credentials dialog)
IDLE → (interval elapsed) → POSTING
IDLE → (QSO logged, per-QSO mode) → POSTING (debounced)
ENABLED/IDLE/ERROR → (user disables) → DISABLED
ENABLED/IDLE/ERROR → (log file closed) → DISABLED
```
