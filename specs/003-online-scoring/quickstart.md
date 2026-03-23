# Quickstart: Online Score Publishing

## Prerequisites
- ContestLogX builds successfully (`make`)
- All existing tests pass (`make test`, `make test-logs`)
- A contestonlinescore.com account (callsign + password)

## Architecture Overview

```
┌──────────────┐     ┌───────────────────┐     ┌─────────────────────┐
│  MainWindow  │────>│ OnlineScoreClient │────>│ contestonlinescore  │
│              │     │                   │     │     .com/post/      │
│  - Timer     │     │ - XML generation  │     │                     │
│  - Menu      │     │ - HTTP POST       │<────│  JSON response      │
│  - Status    │     │ - Auth            │     └─────────────────────┘
│              │<────│ - Signals         │
└──────────────┘     └───────────────────┘
       │                      │
       v                      v
┌──────────────┐     ┌───────────────────┐
│   Settings   │     │  ContestEngine    │
│              │     │                   │
│ - Credentials│     │ - Score breakdown │
│ - Interval   │     │ - Multipliers     │
│ - Club       │     │ - Band stats      │
└──────────────┘     └───────────────────┘
```

## New Files
| File | Purpose |
|------|---------|
| `include/onlineScoreClient.h` | Score posting client class header |
| `src/net/onlineScoreClient.cpp` | XML generation, HTTP POST, response handling |

## Modified Files
| File | Changes |
|------|---------|
| `include/settings.h` | Add online scoring credential getters/setters |
| `src/utils/settings.cpp` | Implement credential storage (XOR + Base64) |
| `include/stationInfo.h` | Add cqZone, ituZone, arrlSection fields |
| `src/core/stationInfo.cpp` | Implement new fields, JSON serialization |
| `include/mainWindow.h` | Add timer, client pointer, menu action, slots |
| `src/ui/mainWindow.cpp` | Menu setup, enable/disable logic, timer, status bar |
| `src/ui/preferencesDialog.cpp` | Add online scoring settings tab/section |
| `contests/*.json` (10 files) | Add contestOnlineScore blocks |
| `CMakeLists.txt` | Add new source files |

## Key Implementation Steps
1. Extend StationInfo with CQ/ITU zone and ARRL section
2. Add online scoring settings to Settings class
3. Add online scoring section to Preferences dialog
4. Create OnlineScoreClient class (XML generation + HTTP POST)
5. Add Contest menu toggle with validation gate
6. Add timer and per-QSO posting triggers in MainWindow
7. Add status bar indicator
8. Update 10 contest definitions with contestOnlineScore blocks
9. Test with actual contestonlinescore.com account

## Build & Test
```bash
make                 # build
make test            # unit tests
make test-logs       # contest log validation (scores unchanged)
./clx --debug        # run with debug logging to verify XML output
```
