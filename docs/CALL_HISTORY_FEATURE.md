# Call History Feature

## Overview

The Call History feature in ContestLogX allows you to maintain a persistent database of callsigns you've worked and their associated operator information (name, QTH, state, ID number, etc.). This information is automatically used to pre-fill exchange fields during contest operation, speeding up QSO entry.

## Features

### 1. Automatic Exchange Pre-Population

When you enter a callsign during a contest:
1. Type the callsign in the CALL field
2. Press TAB or SPACE to move to the next field
3. If call history is **enabled** and the callsign exists in history:
   - Matching exchange fields are automatically populated
   - You can accept the values or modify them
4. If not found in history, the app falls back to the most recent QSO with that call from your current log

### 2. Intelligent Field Matching

The system recognizes various field name formats across different contests:
- **Name fields**: NAME, NAMEs (sent), NAMEr (received)
- **Location fields**: STATE, PROV (province), QTH
- **ID fields**: CWopsID, CWA (CW Academy), ID, SERIAL
- **Grid fields**: GRID
- **Exchange fields**: EXC, EXCs, EXCr

### 3. Automatic History Updates

When you save your contest log:
1. If **auto-save** is enabled, ContestLogX automatically:
   - Extracts static operator information from all QSOs
   - Filters out variable data (date, band, frequency, RST, points)
   - Merges new information with existing history records
   - Saves to `history.json`

### 4. History Management Dialog

Access via **File → Manage Call History**

#### View & Edit
- **Table View**: Shows all records with dynamic columns (columns appear based on what's in your history)
- **Double-click any record** or **right-click → Edit** to modify
- **Edit dialog** shows all fields as editable text inputs
- **Right-click → Delete** to remove a specific record

#### Settings
- **Enable call history insertion**: Toggle whether to use history during contest entry
- **Auto-save call history**: Toggle whether to automatically update history when saving logs
- **Clear All History**: Permanently delete all history records

## Data Storage

### Location
```
Linux:   ~/.local/share/ContestLogX/history.json
macOS:   ~/Library/Application Support/ContestLogX/history.json
Windows: %APPDATA%\ContestLogX\history.json
```

### Format
History is stored as JSON array with flexible field structure:

```json
[
  {
    "CALL": "W5ABC",
    "NAMEs": "JOHN",
    "EXCHs": "12345",
    "STATE": "TX"
  },
  {
    "CALL": "K4XYZ",
    "NAME": "MARY",
    "QTH": "FLORIDA",
    "CWopsID": "5678"
  }
]
```

### What Gets Stored
✅ **Stored (static operator info)**:
- Name/operator info
- QTH/location
- State/province
- ID numbers (CWopsID, CWA, etc.)
- Grid squares
- Exchange info that doesn't vary per-QSO

❌ **Not Stored (per-QSO variable data)**:
- Date/time
- Band/frequency
- Mode
- RST values
- Scoring/points
- Serial numbers (QSO-specific)

## Workflow Examples

### Example 1: CWops Test (CWT)

**First time working W5ABC:**
1. Type "W5ABC" in CALL field, press TAB
2. History lookup: not found, fallback to current log (not there either)
3. Type "JOHN" in NAME field, "12345" in ID field
4. Log the QSO
5. Save log with auto-save enabled
6. **History updated**: W5ABC → {NAME: JOHN, ID: 12345}

**Later, working W5ABC again in same contest:**
1. Type "W5ABC" in CALL field, press TAB
2. History lookup: found! W5ABC with NAME and ID
3. **NAME field auto-filled with "JOHN"**
4. **ID field auto-filled with "12345"**
5. Accept values or modify as needed, log the QSO

### Example 2: Multiple Contests

**ARRL 10m Contest:**
- Works K4XYZ, exchanges NAME="MARY" and STATE="FL"
- Saved to history: K4XYZ → {NAME: MARY, STATE: FL}

**ARRL 160m Contest (next day):**
- Works K4XYZ again, exchanges NAME and GRID
- History lookup finds: K4XYZ with NAME="MARY", STATE="FL"
- NAME field auto-filled with "MARY" (matched!)
- STATE and GRID not in ARRL 160m exchange, so no collision
- Type GRID code, log QSO
- Save with auto-save enabled
- **History merged**: K4XYZ → {NAME: MARY, STATE: FL, GRID: EM78...}

## Settings Persistence

Both settings are saved in your global ContestLogX settings file:
- Enable/disable call history insertion
- Enable/disable auto-save

These settings are preserved between sessions.

## Tips & Best Practices

1. **Start fresh or keep history?**
   - First contest: "Clear All History" if you want only current data
   - Ongoing: Leave history enabled to build over time

2. **Managing conflicts:**
   - If someone's info changes (moved, callsign update, etc.), open the dialog and Edit
   - Right-click → Delete to remove bad entries

3. **Privacy:**
   - History file is local-only (no uploads)
   - You control what stays/goes via Edit or Delete

4. **Performance:**
   - Even with thousands of records, lookup is fast (indexed by call)
   - JSON file stays small (typical: < 1 MB for years of data)

5. **Backup:**
   - Consider backing up `history.json` periodically
   - It's just a text file, easy to copy/restore

## Troubleshooting

**History not showing?**
- Verify "Enable call history insertion" is checked in the dialog
- Check that calls exist in history.json (open the dialog to see)

**Auto-save not working?**
- Verify "Auto-save call history" is checked
- Make sure you're using "Save Log" (Ctrl+S), not just closing the app

**Fields not pre-filling?**
- Contest exchange fields must match history field names
  - Example: If history has "NAME" but contest uses "NAMEr", no auto-fill
  - Solution: Edit the history record to use the contest's field name
- Field names are case-sensitive

**History file corrupt?**
- Delete `history.json` and restart app
- App auto-creates empty history on first save
- Consider opening and clearing from the dialog instead

