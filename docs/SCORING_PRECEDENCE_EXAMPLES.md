# Contest Scoring Precedence Examples

## Overview

ContestLogX supports flexible scoring rules based on geographic relationships. The `precedence` array in the contest JSON determines which scoring rule is applied first.

## Available Scoring Rules

- **sameDxccEntity**: Same DXCC entity (e.g., W1AW to N9OH - both USA)
- **differentDxccEntity**: Different DXCC entities (e.g., W1AW to G3ABC)
- **sameCountry**: Alias for sameDxccEntity (backward compatibility)
- **differentCountry**: Alias for differentDxccEntity (backward compatibility)
- **sameContinent**: Same continent, different DXCC (e.g., W1AW to VE3XYZ)
- **differentContinent**: Different continents (e.g., W1AW to JA1ABC)

## How Precedence Works

The contest engine evaluates each rule in the `precedence` array order. **The first matching rule is used**.

**Critical:** Only rules listed in the `precedence` array will be evaluated. If you define a scoring rule in the `points` section but omit it from `precedence`, it will be **completely ignored** and you'll get a warning in the debug log.

### Example 1: DXCC-Focused Contest

Prioritize DXCC entity differences over continents:

```json
{
  "scoring": {
    "points": {
      "sameDxccEntity": {"CW": 1, "SSB": 1},
      "differentDxccEntity": {"CW": 3, "SSB": 2}
    },
    "precedence": [
      "sameDxccEntity",
      "differentDxccEntity"
    ]
  }
}
```

**Result:**
- W1AW → N9OH: 1 point (same DXCC, matches first rule)
- W1AW → VE3XYZ: 3 points (different DXCC, matches second rule)
- W1AW → G3ABC: 3 points (different DXCC, matches second rule)

### Example 2: Continent-Focused Contest

Prioritize continents over DXCC entities:

```json
{
  "scoring": {
    "points": {
      "sameContinent": {"CW": 2, "SSB": 1},
      "differentContinent": {"CW": 5, "SSB": 3},
      "sameDxccEntity": {"CW": 1, "SSB": 1}
    },
    "precedence": [
      "sameContinent",
      "differentContinent",
      "sameDxccEntity"
    ]
  }
}
```

**Result:**
- W1AW → N9OH: 2 points (same continent, matches first rule)
- W1AW → VE3XYZ: 2 points (same continent, matches first rule)
- W1AW → G3ABC: 5 points (different continent, matches second rule)

Note: Even though W1AW → N9OH is same DXCC, it scores 2 points because `sameContinent` is checked first!

### Example 3: Complex Multi-Level Scoring

Separate domestic, same-continent, and DX scoring:

```json
{
  "scoring": {
    "points": {
      "sameDxccEntity": {"CW": 1, "SSB": 1},
      "sameContinent": {"CW": 2, "SSB": 1},
      "differentContinent": {"CW": 5, "SSB": 3}
    },
    "precedence": [
      "sameDxccEntity",
      "sameContinent",
      "differentContinent"
    ]
  }
}
```

**Result:**
- W1AW → N9OH: 1 point (same DXCC, matches first rule)
- W1AW → VE3XYZ: 2 points (different DXCC but same continent NA, matches second rule)
- W1AW → G3ABC: 5 points (different continent, matches third rule)

### Example 4: Backward Compatible (Default)

If no `precedence` is specified, uses this default order:

```json
{
  "precedence": [
    "sameDxccEntity",
    "sameCountry",
    "differentDxccEntity",
    "differentCountry",
    "sameContinent",
    "differentContinent"
  ]
}
```

This maintains backward compatibility with older contest definitions using `sameCountry` and `differentCountry`.

### Example 5: Ignoring Defined Rules

What happens if you define rules but don't include them in precedence?

```json
{
  "scoring": {
    "points": {
      "sameDxccEntity": {"CW": 1, "SSB": 1},
      "differentDxccEntity": {"CW": 3, "SSB": 2},
      "sameContinent": {"CW": 2, "SSB": 1},
      "differentContinent": {"CW": 5, "SSB": 3}
    },
    "precedence": [
      "sameDxccEntity",
      "differentDxccEntity"
    ]
  }
}
```

**Result:**
- W1AW → N9OH: 1 point (same DXCC, first rule matches)
- W1AW → VE3XYZ: 3 points (different DXCC, second rule matches)
- W1AW → G3ABC: 3 points (different DXCC, second rule matches)

**Note:** Even though `sameContinent` and `differentContinent` are defined in `points`, they are **never checked** because they're not in the `precedence` array. You'll see warnings in the debug log:
```
WARNING: Scoring rule 'sameContinent' is defined in points but not in precedence array - it will be ignored!
WARNING: Scoring rule 'differentContinent' is defined in points but not in precedence array - it will be ignored!
```

## Best Practices

1. **Always include precedence array** for clarity, even if using default order
2. **Include all defined rules** - Don't define rules in `points` that you don't list in `precedence`
3. **Test your precedence** with example QSOs to ensure correct scoring
4. **Document the logic** in the contest JSON comments
5. **Check debug logs** during testing to catch configuration warnings

## Testing Your Configuration

Use these test cases:

| My Call | Their Call | Relationship | Use For Testing |
|---------|------------|--------------|-----------------|
| W1AW | N9OH | Same DXCC (USA) | sameDxccEntity |
| W1AW | VE3XYZ | Different DXCC, Same Continent (NA) | sameContinent |
| W1AW | XE1ABC | Different DXCC, Same Continent (NA) | sameContinent |
| W1AW | G3ABC | Different DXCC, Different Continent | differentContinent/differentDxccEntity |
| W1AW | JA1ABC | Different DXCC, Different Continent | differentContinent/differentDxccEntity |
