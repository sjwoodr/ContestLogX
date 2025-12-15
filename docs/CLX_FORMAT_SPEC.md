# .clx 2nd Generation Contest File Format (.clx)

## Overview

A modern, cross-platform contest log format based on JSON. A human-readable, version-control-friendly format.

## File Extension

`.clx` - CLX 2nd Generation format

## Format Structure

### Top Level

```json
{
  "version": "1.0",
  "format": ".clx",
  "created": "2024-12-14T04:20:00Z",
  "modified": "2024-12-14T05:30:00Z",
  "contest": { ... },
  "station": { ... },
  "bands": [ ... ],
  "exchange_fields": [ ... ],
  "qsos": [ ... ],
  "cw_messages": { ... },
  "notes": [ ... ],
  "statistics": { ... }
}
```

## Detailed Sections

### 1. Contest Information

```json
"contest": {
  "name": "ARRL Sweepstakes",
  "type": "ARRL_SS",
  "mode": "CW",
  "year": 2024,
  "start_time": "2024-11-02T21:00:00Z",
  "end_time": "2024-11-04T03:00:00Z",
  "categories": {
    "power": "HIGH",
    "operator": "SINGLE_OP",
    "band": "ALL",
    "assisted": false
  },
  "rules_version": "2024"
}
```

### 2. Station Information

```json
"station": {
  "callsign": "N9OH",
  "operator": "Steve",
  "location": {
    "grid": "EN72",
    "state": "OH",
    "county": "Franklin",
    "cq_zone": 5,
    "itu_zone": 8
  },
  "equipment": {
    "rig": "Icom IC-7300",
    "antenna": "Dipole @ 50ft",
    "power": 1500
  }
}
```

### 3. Band Definitions

```json
"bands": [
  {
    "name": "160m",
    "frequency_min": 1800000,
    "frequency_max": 2000000,
    "default_frequency": 1850000,
    "modes": ["CW", "SSB", "DIGITAL"]
  },
  {
    "name": "80m",
    "frequency_min": 3500000,
    "frequency_max": 4000000,
    "default_frequency": 3550000,
    "modes": ["CW", "SSB", "DIGITAL"]
  }
]
```

### 4. Exchange Field Definitions

Replaces the binary `exfa_stru` format:

```json
"exchange_fields": [
  {
    "name": "serial",
    "label": "SNT",
    "width": 4,
    "type": "number",
    "required": true,
    "attributes": ["prompt", "auto_increment"],
    "validation": {
      "min": 1,
      "max": 9999
    }
  },
  {
    "name": "callsign",
    "label": "CALL",
    "width": 12,
    "type": "callsign",
    "required": true,
    "attributes": ["callsign", "prompt", "dupe_check", "upper_case"]
  },
  {
    "name": "rst_sent",
    "label": "RST",
    "width": 3,
    "type": "rst",
    "required": true,
    "attributes": ["prompt"],
    "default": "599"
  },
  {
    "name": "section",
    "label": "SEC",
    "width": 3,
    "type": "string",
    "required": true,
    "attributes": ["prompt", "multiplier", "upper_case"],
    "validation": {
      "pattern": "[A-Z]{2,3}",
      "lookup": "arrl_sections"
    }
  }
]
```

**Attributes:**
- `prompt` - Show in QSO entry form
- `callsign` - This is the callsign field
- `dupe_check` - Use for duplicate checking
- `multiplier` - This is a multiplier field
- `required` - Must be filled
- `upper_case` - Force uppercase
- `auto_increment` - Auto-increment (serial numbers)
- `hidden` - Don't display
- `read_only` - Can't edit

### 5. QSO Records

```json
"qsos": [
  {
    "id": 1,
    "timestamp": "2024-11-02T21:15:32Z",
    "frequency": 14025500,
    "band": "20m",
    "mode": "CW",
    "callsign": "K1ABC",
    "duplicate": false,
    "exchange_sent": {
      "serial": 1,
      "rst": "599",
      "section": "OH"
    },
    "exchange_received": {
      "serial": 123,
      "rst": "599",
      "section": "MA",
      "precedence": "A",
      "check": "72",
      "name": "JOHN"
    },
    "points": 2,
    "multiplier_new": ["MA"],
    "flags": {
      "confirmed": true,
      "edited": false,
      "deleted": false
    },
    "metadata": {
      "operator": "N9OH",
      "station": 1,
      "computer": "main"
    }
  }
]
```

### 6. CW Messages

```json
"cw_messages": {
  "f1": "CQ TEST N9OH N9OH",
  "f2": "@ 5NN #",
  "f3": "TU N9OH",
  "f4": "@",
  "f5": "?",
  "f6": "AGN?",
  "f7": "QRZ?",
  "f8": "N9OH",
  "insert_1": "R",
  "insert_2": "NR?"
}
```

### 7. Notes

```json
"notes": [
  {
    "id": 1,
    "timestamp": "2024-11-02T22:30:00Z",
    "type": "contest",
    "text": "Band conditions improving on 20m",
    "category": "propagation"
  },
  {
    "id": 2,
    "qso_id": 45,
    "timestamp": "2024-11-03T01:15:00Z",
    "type": "qso",
    "text": "Weak signal, confirmed exchange multiple times"
  }
]
```

### 8. Statistics

```json
"statistics": {
  "total_qsos": 456,
  "duplicate_qsos": 12,
  "total_points": 912,
  "multipliers": {
    "sections": 78,
    "total": 78
  },
  "score": 71136,
  "by_band": {
    "160m": {"qsos": 23, "points": 46, "mults": 15},
    "80m": {"qsos": 67, "points": 134, "mults": 42},
    "40m": {"qsos": 145, "points": 290, "mults": 65},
    "20m": {"qsos": 178, "points": 356, "mults": 71},
    "15m": {"qsos": 32, "points": 64, "mults": 28},
    "10m": {"qsos": 11, "points": 22, "mults": 10}
  },
  "by_mode": {
    "CW": {"qsos": 456, "points": 912}
  },
  "rate": {
    "current_hour": 45,
    "last_hour": 52,
    "last_10_minutes": 8
  }
}
```

## Complete Example

```json
{
  "version": "1.0",
  "format": ".clx",
  "created": "2024-11-02T21:00:00Z",
  "modified": "2024-11-04T03:00:00Z",
  
  "contest": {
    "name": "ARRL Sweepstakes",
    "type": "ARRL_SS",
    "mode": "CW",
    "year": 2024
  },
  
  "station": {
    "callsign": "N9OH",
    "operator": "Steve",
    "location": {
      "grid": "EN72",
      "state": "OH"
    }
  },
  
  "bands": [
    {"name": "160m", "frequency_min": 1800000, "frequency_max": 2000000},
    {"name": "80m", "frequency_min": 3500000, "frequency_max": 4000000},
    {"name": "40m", "frequency_min": 7000000, "frequency_max": 7300000},
    {"name": "20m", "frequency_min": 14000000, "frequency_max": 14350000}
  ],
  
  "exchange_fields": [
    {
      "name": "serial",
      "label": "SNT",
      "type": "number",
      "required": true,
      "attributes": ["prompt", "auto_increment"]
    },
    {
      "name": "callsign",
      "label": "CALL",
      "type": "callsign",
      "required": true,
      "attributes": ["callsign", "prompt", "dupe_check"]
    },
    {
      "name": "section",
      "label": "SEC",
      "type": "string",
      "required": true,
      "attributes": ["prompt", "multiplier"]
    }
  ],
  
  "qsos": [
    {
      "id": 1,
      "timestamp": "2024-11-02T21:15:32Z",
      "frequency": 14025500,
      "band": "20m",
      "mode": "CW",
      "callsign": "K1ABC",
      "duplicate": false,
      "exchange_sent": {
        "serial": 1,
        "rst": "599",
        "section": "OH"
      },
      "exchange_received": {
        "serial": 123,
        "rst": "599",
        "section": "MA"
      },
      "points": 2,
      "multiplier_new": ["MA"]
    }
  ],
  
  "cw_messages": {
    "f1": "CQ TEST N9OH N9OH",
    "f2": "@ 5NN #",
    "f3": "TU N9OH"
  },
  
  "statistics": {
    "total_qsos": 1,
    "total_points": 2,
    "multipliers": {"sections": 1},
    "score": 2
  }
}
```

## Benefits Over .wl Format

### 1. **Human Readable**
- Can view/edit in any text editor
- Easy to understand structure
- Great for debugging

### 2. **Cross-Platform**
- No Windows-specific structures
- No byte-order issues
- No pointer problems
- Works on Linux/Mac/Windows

### 3. **Version Control Friendly**
- Text-based format
- Git can track changes
- Easy to diff
- Merge conflicts are manageable

### 4. **Extensible**
- Add new fields without breaking old readers
- Contest-specific fields go in exchange
- Future-proof design

### 5. **No External Dependencies**
- No OLE libraries needed
- No COM objects
- Qt has built-in JSON support

### 6. **Validation**
- JSON Schema validation
- Type checking
- Easy error detection

### 7. **Tooling**
- jq for command-line queries
- Any JSON library can read it
- Easy to write converters

## File Size Considerations

JSON is larger than binary, but:

- **Compression:** `.clx.gz` reduces size 80-90%
- **Typical contest:** 1000 QSOs ≈ 500KB uncompressed, 50KB compressed
- **Modern disks:** Size is not a concern
- **Trade-off:** Readability > size

## Migration Path

### From .wl to .clx

Create a Windows converter that:
1. Opens .wl file using .clx COM
2. Extracts all data
3. Writes .clx JSON

### From ADIF to .clx

ADIF → .clx converter:
- Read ADIF
- Map fields to JSON structure
- Add contest-specific metadata
- Write .clx

### Dual Format Support

.clx Qt can support both:
- **Import:** ADIF, CSV, .clx
- **Export:** ADIF, CSV, .clx, Cabrillo
- **Native:** .clx

## Implementation Plan

### Phase 1: Core Format (3-4 days)

1. **Define JSON schema** ✓ (done above)
2. **Implement reader/writer**
   - QJsonDocument for parsing
   - Validate structure
   - Error handling
3. **Data model classes**
   - ContestInfo
   - ExchangeFieldDef
   - QsoRecord (enhanced)

### Phase 2: UI Integration (2-3 days)

1. **Contest selection dialog**
   - Choose contest type
   - Set station info
   - Configure exchange fields
2. **Dynamic exchange form**
   - Build UI from exchange_fields
   - Validation
   - Auto-increment serials
3. **Save/Load .clx files**

### Phase 3: Contest Modules (Per Contest)

Each contest needs:
1. JSON definition file
2. Exchange field layout
3. Scoring rules
4. Multiplier logic

Start with popular contests:
- ARRL Sweepstakes
- CQ WW DX
- ARRL Field Day
- NA QSO Party
- CQ WPX

### Phase 4: Conversion Tools (1-2 days)

1. **ADIF → .clx**
2. **CSV → .clx**
3. **Windows: .wl → .clx** (using .clx COM)

## File Format Versioning

```json
"version": "1.0"
```

**Version 1.0:** Initial format (what's specified here)

**Future versions:**
- Add new fields (backwards compatible)
- Use semantic versioning
- Old readers can ignore unknown fields
- New readers can handle old files

## Example Contest Definitions

### ARRL Sweepstakes

```json
{
  "contest_type": "ARRL_SS",
  "exchange_fields": [
    {"name": "serial_sent", "type": "number"},
    {"name": "precedence", "type": "string", "values": ["A", "B", "M", "Q", "S", "U"]},
    {"name": "check", "type": "number", "width": 2},
    {"name": "section", "type": "string", "multiplier": true}
  ],
  "scoring": {
    "points_per_qso": 2,
    "multipliers": ["section"]
  }
}
```

### CQ WW DX

```json
{
  "contest_type": "CQ_WW_DX",
  "exchange_fields": [
    {"name": "rst_sent", "type": "rst"},
    {"name": "cq_zone", "type": "number", "multiplier": true}
  ],
  "scoring": {
    "points_per_qso": {"same_continent": 1, "different_continent": 3},
    "multipliers": ["cq_zone", "country"]
  }
}
```

## Conclusion

The `.clx` format provides:

✅ Modern, cross-platform design  
✅ Human-readable JSON  
✅ Flexible exchange field system  
✅ Contest-agnostic structure  
✅ Easy to implement (days, not weeks)  
✅ No Windows dependencies  
✅ Version control friendly  
✅ Extensible for future needs  

**This is the right format for ContestLogX!**

---

*Format Version: 1.0*  
*Last Updated: 2024-12-14*  
*Status: PROPOSED*
