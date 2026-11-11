# nmea_parser — NMEA 0183 sentence parser

**Status: candidate**
**Layer: L2 (protocols)**

Pure-software parser for NMEA 0183 GPS/GNSS sentences. Supports GGA, RMC, and GSA.
Zero hardware dependency — feed null-terminated sentence strings, get structured position/time/quality data out.

## Origin

Reimplemented for the needs of an internal project (Air780EG GNSS NMEA handling).
Decoupled: modem interaction, quality gating, filtering stay in the project; pure sentence parsing moves to this wheel.

## Why self-written?

Searched GitHub (2026-05-02). Referenced kosma/minmea (WTFPL). De facto standard embedded NMEA parser. Self-written for platform convention consistency; implementation verified against NMEA 0183 spec (checksum XOR, GGA/RMC/GSA field positions, talker ID handling).

## Dependencies

None. Pure C99, no external dependency.

## API

```c
nmea_parser_data_t data;
nmea_parser_data_init(&data);

/* Typical GNSS cycle: multiple sentences per fix */
if (nmea_parser_verify_checksum(line)) {
    nmea_parser_parse(line, &data);
    /* data accumulates across GGA + RMC + GSA sentences */
}

if (data.fix_valid && data.fix_mode == 2) {
    printf("3D fix: %.6f, %.6f, alt=%.1fm\n",
           data.latitude_deg, data.longitude_deg, data.altitude_m);
}
```

## Configuration (compile-time)

No compile-time configuration needed.

## Supported platforms

- [x] Host (sim) — passed 2026-04-28
- [ ] GD32F405 (flybox) — original project code verified, wheel not separately HIL tested

## Known issues

- Only handles GGA, RMC, GSA. Other sentence types (VTG, GLL, ZDA) return `NMEA_PARSER_ERR_UNKNOWN`.
- Coordinates are single-precision floats in degrees, so they move in steps of about 0.85 m in latitude
  and up to 1.7 m in longitude (2^-17 and 2^-16 degree). That is enough for consumer GNSS, not for RTK.
- Year field uses 80-threshold: YY≥80 → 19XX, YY<80 → 20XX (standard NMEA convention).
- `nmea_parser_parse()` verifies the checksum itself and returns `NMEA_PARSER_ERR_CHECKSUM` on a mismatch, so a sentence without a valid `*HH` is refused.

## How to test

```bash
cd wheels/protocols/nmea_parser/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_nmea_parser
```

203 assertions across 62 test cases: checksum validation (valid/invalid/null/lowercase-hex/truncated-star), sentence identification (GP/GN/GL/BD prefixes), GGA parsing (position/quality/sats/hdop/altitude), RMC parsing (lat/lon/speed/course/date/time/fix/truncated), GSA parsing (2D/3D/nofix/DOP), null safety, unknown sentence handling, data init, south/west coordinates, equator + prime meridian boundary, cross-sentence accumulation, re-init clears state, no-comma rejection, max lat/lon (90/180), year boundary (00→2000, 79→2079, 80→1980, 99→1999), GGA no-fix empty fields, RMC empty speed/course, alternate talker IDs, speed knots→km/h conversion, value limits, plain decimals only, long digit fields, bit-for-bit match with strtof.

## Test status

- Unit tests (sim): PASS (62 C tests, 203 assertions)
- HIL: N/A

## API convention deviations

None.

## Protocol

NMEA 0183 sentence format: `$TTDDD,field,field,...*HH\r\n`

| Sentence | Fields extracted |
|----------|----------------|
| GGA | lat/lon, fix_quality, satellites, hdop, altitude |
| RMC | time, date, fix_valid, lat/lon, speed, course |
| GSA | fix_mode (2D/3D), pdop, hdop, vdop |

Time and date come from RMC only. GGA carries a time too, but a consumer pairs the time
with the date and with the moment the RMC arrived, so a GGA-only stream has a position
and no time.

Supports both GP (GPS-only) and GN (multi-constellation) talker prefixes.

A valid checksum does not vouch for a value, so each one has to fit these limits:

| Field | Accepted |
|-------|----------|
| GGA latitude, longitude | ddmm.mmmm to 90 degrees, dddmm.mmmm to 180, minutes under 60, with N/S or E/W |
| time | hour 0-23, minute 0-59, second 0-60 (leap second) |
| date | day 1-31, month 1-12 |
| speed | 0 to 1000 knots |
| course | 0 to 360 degrees |
| altitude | -1000000 to 1000000 m |
| HDOP, PDOP, VDOP | 0 to 1000 |
| fix quality | 0 to 9 |
| satellites | 0 to 99 |

A value outside them is ignored: its field keeps what it had, a bad course also clears
`course_valid`, and the call still returns `NMEA_PARSER_OK`. An empty field still counts
as 0, except a GGA position field, which keeps the old value. In RMC, latitude and
longitude out of range still parse to 0.

A decimal field is a sign, digits and at most one point, up to the field end. Anything
else in it (`1e2`, `0x10`, `12abc`, `nan`, `inf`, a space) counts as out of range. Digits
past the 15th are dropped, which can move the result by one float step at most, and
before the point they put the number past every limit. The parser uses no heap and no
libc float parsing, and for fields a receiver sends it gives what `strtof` gives.

## API review

- **Date**: 2026-04-28
- **Key decisions**: Accumulative data struct (caller feeds GGA+RMC+GSA, fields accumulate); parsing verifies the checksum, and the check is also public on its own; single-precision float coordinates (sufficient for consumer GNSS); supports GP and GN talker prefixes via 2-char skip.

## Responsible human

@L-X-Yao
