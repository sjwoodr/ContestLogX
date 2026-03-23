# Contract: contestonlinescore.com XML Post Format

## Endpoint
- **URL**: `https://contestonlinescore.com/post/`
- **Method**: HTTP POST
- **Auth**: Basic HTTP Authentication (callsign = username, password = password)
- **Content-Type**: `application/xml`

## Request Body: dynamicresults XML

```xml
<?xml version="1.0"?>
<dynamicresults>
  <contest>{contestId}</contest>
  <call>{operator callsign}</call>
  <ops>{operator callsign(s), comma-separated}</ops>
  <soft>ContestLogX</soft>
  <version>{APP_VERSION}</version>
  <class power="{HIGH|LOW|QRP}"
         assisted="{NON-ASSISTED|ASSISTED}"
         transmitter="{ONE|TWO|UNLIMITED}"
         ops="{SINGLE-OP|MULTI-OP}"
         bands="{ALL|160M|80M|...}"
         mode="{MIXED|CW|PH|RTTY|DIGI}"
         overlay="{N/A|TB-WIRES|ROOKIE|CLASSIC|WIRE-ONLY}">
  </class>
  <club>{club name}</club>
  <qth>
    <dxcccountry>{DXCC primary prefix}</dxcccountry>
    <cqzone>{CQ zone number}</cqzone>
    <iaruzone>{ITU zone number}</iaruzone>
    <arrlsection>{ARRL section}</arrlsection>
    <stprvoth>{state/province}</stprvoth>
    <grid6>{6-char grid}</grid6>
  </qth>
  <breakdown>
    <!-- Per band/mode entries -->
    <qso band="20" mode="CW">177</qso>
    <mult band="20" mode="CW" type="state">72</mult>
    <point band="20" mode="CW">290</point>

    <!-- For two multiplier types -->
    <mult band="20" mode="PH" type="zone">36</mult>
    <mult band="20" mode="PH" type="country">118</mult>

    <!-- Required totals row -->
    <qso band="total" mode="ALL">{total QSOs}</qso>
    <mult band="total" mode="ALL" type="{mult1type}">{total mult1}</mult>
    <mult band="total" mode="ALL" type="{mult2type}">{total mult2}</mult>
    <point band="total" mode="ALL">{total points}</point>
  </breakdown>
  <score>{final contest score}</score>
  <timestamp>{YYYY-MM-DD HH:MM:SS UTC}</timestamp>
</dynamicresults>
```

## Response Format (JSON in HTML body)

### Success
```json
{"status":200,"status_message":"OK-Full"}
```

### Errors
```json
{"status":404,"status_message":"Error! Contest is closed or is not valid"}
{"status":405,"status_message":"Error! Empty call"}
```

## Mode Mapping
| CLX Mode | XML Mode |
|----------|----------|
| CW       | CW       |
| SSB/USB/LSB | PH    |
| RTTY     | RY       |
| FT8/FT4/PSK/DIGI | DG |
| FM       | PH       |

## Band Format
Bands use numeric-only format without "m" suffix: "160", "80", "40", "20", "15", "10", "6", "2".

## Multiplier Type Identifiers (lowercase only)
zone, country, state, gridsquare, wpxprefix, prefix, hq
