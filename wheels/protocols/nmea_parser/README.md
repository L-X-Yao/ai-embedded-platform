# nmea_parser — NMEA 0183 sentence parser

**Status: candidate**
**Layer: L2 (protocols)**

Pure-software parser for NMEA 0183 GPS/GNSS sentences. Supports GGA, RMC, and GSA.
Zero hardware dependency — feed null-terminated sentence strings, get structured position/time/quality data out.

## Why not VSF native?

Searched `vsf/source/service/`, `vsf/source/component/`, `vsf/source/utilities/` for:
`nmea`, `gps`, `gnss`, `gga`, `rmc` — zero results.

VSF has no NMEA parsing capability.

## Origin

Extracted from `projects/flybox/src/app/gnss_task.c` (Air780EG GNSS NMEA handling).
Decoupled: modem interaction, quality gating, filtering stay in the project; pure sentence parsing moves to this wheel.

## Protocol

NMEA 0183 sentence format: `$TTDDD,field,field,...*HH\r\n`

| Sentence | Fields extracted |
|----------|----------------|
| GGA | fix_quality, satellites, hdop, altitude |
| RMC | time, date, fix_valid, lat/lon, speed, course |
| GSA | fix_mode (2D/3D), pdop, hdop, vdop |

Supports both GP (GPS-only) and GN (multi-constellation) talker prefixes.

## API

```c
nmea_data_t data;
nmea_data_init(&data);

/* Typical GNSS cycle: multiple sentences per fix */
if (nmea_verify_checksum(line)) {
    nmea_parse(line, &data);
    /* data accumulates across GGA + RMC + GSA sentences */
}

if (data.fix_valid && data.fix_mode == 2) {
    printf("3D fix: %.6f, %.6f, alt=%.1fm\n",
           data.latitude, data.longitude, data.altitude_m);
}
```

## Dependencies

None. Pure C99, no external dependency.

## Configuration (compile-time)

No compile-time configuration needed.

## Supported platforms

- [x] Host (sim) — passed 2026-04-28
- [ ] GD32F405 (flybox) — original project code verified, wheel not separately HIL tested

## Known issues

- Only handles GGA, RMC, GSA. Other sentence types (VTG, GLL, ZDA) return `NMEA_ERR_UNKNOWN`.
- Coordinate parsing uses single-precision float (~7 significant digits ≈ ~1cm at equator). Sufficient for consumer GNSS.
- Year field uses 80-threshold: YY≥80 → 19XX, YY<80 → 20XX (standard NMEA convention).
- `nmea_parse()` does not verify checksum internally — caller should call `nmea_verify_checksum()` first if data integrity matters.

## Tests

```bash
cd wheels/protocols/nmea_parser/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_nmea_parser
```

77 assertions across 24 test cases: checksum validation (valid/invalid/null/lowercase-hex), sentence identification (GP/GN prefixes), GGA parsing (quality/sats/hdop/altitude), RMC parsing (lat/lon/speed/course/date/time/fix), GSA parsing (2D/3D/nofix/DOP), null safety, unknown sentence handling, data init, south/west coordinates, equator + prime meridian boundary, cross-sentence accumulation, re-init clears state, no-comma rejection, max lat/lon (90/180), year boundary (00→2000, 79→2079, 80→1980, 99→1999), GGA no-fix empty fields, RMC empty speed/course.

## Test status

- Unit tests (sim): PASS (24 C tests, 77 assertions)
- HIL: N/A

## Convention Deviations

None.

## API review

- **Date**: 2026-04-28
- **Key decisions**: Accumulative data struct (caller feeds GGA+RMC+GSA, fields accumulate); checksum verification is separate from parsing (caller decides); single-precision float coordinates (sufficient for consumer GNSS); supports GP and GN talker prefixes via 2-char skip.
