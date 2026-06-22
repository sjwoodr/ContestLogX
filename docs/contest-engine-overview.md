# Contest Engine Overview

High-level summary of how ContestLogX loads and processes user-defined contest definitions.

---

## Architecture in One Sentence

A contest JSON file is loaded once into `ContestEngine`, which then drives all scoring, dupe detection, validation, and multiplier tracking for the life of that log session - no recompilation required.

---

## Key Files

| File | Role |
|------|------|
| `src/contestEngine.cpp` | Core engine (~2000 lines) - scoring, dupes, multipliers, validation |
| `include/contestEngine.h` | Public interface - all methods and data structures |
| `include/scoringWorker.h` | Background thread wrapper for rescoring large logs |
| `src/core/fileHandler.cpp` | Loads `.json` contest definitions and `.clx` log files |
| `contests/*.json` | User-defined contest definitions (drop-in, no rebuild needed) |

---

## Lifecycle

```
contests/*.json
      │
      ▼
fileHandler loads JSON
      │
      ▼
ContestEngine::loadContest()
  • Parses and caches all JSON properties
  • Builds QSet<QString> of valid multipliers (O(1) lookups)
  • Validates that scoring.precedence covers all defined point rules
      │
      ▼
  Operator logs QSOs
      │
      ▼
ContestEngine::updateRunningScore()   ← triggered on every QSO add/edit
  • isDupe()          → mark dupe, skip scoring
  • calculatePoints() → assign points to QSO record
  • getQsoMultiplierCredit() → determine if any new multipliers were earned
      │
      ▼
  ScoreWidget / MultiplierPanel update
```

---

## Loading a Contest Definition

`loadContest()` does three things:

1. **Caches frequently-read JSON properties** - avoids repeated JSON parsing on every QSO. Band ranges, scoring rules, and dupe scope are all stored in C++ member variables after the first load.

2. **Builds multiplier lookup sets** - `validation.namedMults`, `validation.namedCallPrefixes`, and `validation.inStateMults` are loaded into `QSet<QString>` for fast membership checks during scoring.

3. **Validates the definition itself** - warns if any rule listed in `scoring.points` is absent from `scoring.precedence` (rules not in precedence are silently never applied, so this catches authoring errors).

---

## Scoring

### QSO Points

For each QSO, `calculatePoints()` works through a sequence of checks:

1. **Out-of-band?** - if the frequency doesn't fall within the band's defined range, 0 points.
2. **Invalid partner?** - some contests forbid certain station pairings (e.g., ARRL DX: W/VE stations may not work other W/VE). Checked via `scoring.invalidPartners`.
3. **Geographic relationship** - both callsigns are looked up in the DXCC database; the engine determines whether they are in the same country, same continent, or different continents.
4. **Precedence walk** - the engine steps through `scoring.precedence` in order and applies the points from the **first matching rule**. Rules not reached are never applied.
5. **Mode modifier** - each rule carries per-mode point values (`CW`, `SSB`, `DIGITAL`), so the mode of the QSO selects the right value.

Special cases handled:
- **Flat per-QSO** (`"perQso": 1`) - all QSOs score equally regardless of geography.
- **By mode category** (`"phone"`, `"cw"`, `"digital"`) - for emergency/field-day style contests.
- **By band and month** (`"byBand"`) - ARRL VHF, where points vary by band and the contest's operating month (selected by the operator at log-start via `userPrompts`).

### Multipliers

The multiplier type (from `scoring.multipliers.type`) controls the scope in which each multiplier is counted:

| Type | What counts as "new" |
|------|----------------------|
| `multsOnce` | First time ever worked in the contest |
| `multsPerBand` | First time worked on each band |
| `multsPerMode` | First time worked on each mode |
| `multsPerBandAndMode` | First time worked on each band+mode combination |
| `objectiveMultipliers` | User-selected achievement checkboxes (Winter Field Day) |

Multiplier **categories** define what *qualifies* as a multiplier:

- `namedMults` - states, provinces, counties, or any custom list in `validation.namedMults`
- `dxcc` - DXCC entities resolved from the worked callsign
- `namedCallPrefixes` - specific call prefixes (e.g., YB contests)
- `gridSquares` - Maidenhead grid squares (VHF contests)
- `callsign` - the worked callsign itself counts (CWops CWT)

Some contests mix categories per station class (e.g., ARRL DX: W/VE stations count DXCC entities; DX stations count US states/provinces).

Notable edge-case flags:
- `alaskaAndHawaiiCountDxcc` - when true, AK/HI count as both a state *and* a DXCC entity.
- `usAndCanadaCountDxcc` - when false (ARRL DX default), W/VE callsigns are excluded from DXCC multiplier credit.

#### Multiplier aliasing

Two layered remap mechanisms run before the engine's mult-lookup, used when a received exchange code needs to be rewritten before it counts:

1. **`validation.namedMultAliases`** - an unconditional 1:1 key→value table. Always applied regardless of operator. Use for input forgiveness or scoring-equivalent codes (e.g., `"DC" → "MD"`).

2. **`multAliases`** (top-level, conditional) - an array of rules triggered by the operator's `userPrompts` answer. Each rule names a source set and a mapping. The source set is either a named list (`sourceList: "inStateMults"` or `"namedMults"`) or an inline array of exact values (`sourceValues: ["AIR","BAT",…]`). Two mapping forms are supported:
   - **Static**: `mapsTo` replaces the rawMult with a fixed string (e.g., FQP: every FL county → `FL` for FL operators; CPQP: each prairie province's FED codes → its 2-letter province code, with one rule per province via `sourceValues`).
   - **Prefix**: `mapByPrefix: N` returns `rawMult.left(N)` (e.g., 7QP: `WYALB` → `WY` for 7th-area operators, where the 5-letter exchange's first 2 chars name the state).

   The trigger can be a single value (`promptValue: "FL"`) or any-of (`promptValueIn: ["AZ","ID",…]`). Values in a rule's source set are also hidden from the multiplier panel display when the rule is active, since the operator is earning credit for the aliased target instead.

Both mechanisms are documented in detail in `docs/contest-module-format.md`.

### Final Score

Evaluated by `scoring.finalScore`, a formula string with tokens:

```
SUM(points) * SUM(multipliers)
SUM(points) * (namedMults + dxccMultipliers)
(SUM(points)) * (objectiveMultiplierCount + 1)
SUM(points)   // no multipliers
```

---

## Dupe Checking

`isDupe()` scans the existing QSO list and applies the scope defined by `dupeChecking.type`:

| Type | Rule |
|------|------|
| `overall` | Same callsign anywhere in the log |
| `perBand` | Same callsign on the same band |
| `perMode` | Same callsign on the same mode |
| `perBandAndMode` | Same callsign on the same band **and** mode |
| `perBandAndGridSquare` | Same callsign on same band + grid square (VHF rovers) |

Dupes are marked on the `QsoRecord` and excluded from scoring. The dupe flag is recalculated on every `updateRunningScore()` pass - editing or deleting a QSO can "un-dupe" later contacts automatically.

---

## Validation

Two levels:

1. **Field-level** (`validateExchange()`) - checks a single exchange field value against its type rules. Supported `exchangeValidation.type` values: `nameAndMultiplier`, `namedMultOrSerial`, `namedMultOrPower`, `maidenheadGrid`, `serial`, `freeForm`.

2. **QSO-level** (`validateQso()`) - confirms all required fields are present and that band/mode are valid for the contest.

`validation.receivedExchangeFilter` provides contest-specific runtime filtering: some contests restrict which multiplier codes are valid for a given operator type. The applicable filter is selected by matching the operator's `userPrompts` answer to a rule map.

---

## Scoring Recalculation & Threading

`updateRunningScore()` is called every time the QSO list changes. It does a full pass:

1. Resets all running totals and worked-multiplier sets.
2. Iterates QSOs in log order, computing dupe status, points, and multiplier credit for each.
3. Accumulates `ContestScore` (per-band/mode breakdowns, total points, total mults, final score).
4. Emits updated data to `ScoreWidget` and the multiplier panel.

For large logs, `ScoringWorker` runs this pass on a background thread to keep the UI responsive, emitting a `scoringComplete()` signal when finished.

---

## Adding a New Contest

No C++ code is needed. Create `contests/my_contest.json` following the [contest module format](./contest-module-format.md) and restart ContestLogX - it appears in the contest selection dialog automatically. The engine's behavior (points, dupes, multipliers, validation) is entirely driven by the JSON.
