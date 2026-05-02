# syslog_packet

**STATUS: candidate**
**Layer: L2 (protocols)**

RFC 5424 syslog message formatter. Produces
`<PRI>VERSION TIMESTAMP HOSTNAME APP-NAME PROCID MSGID STRUCTURED-DATA MSG`
with NILVALUE "-" for missing fields. Timestamp as ISO 8601 UTC if provided,
"-" if zero. Formatter only -- application handles UDP/TCP transport.
## API

```c
#include "syslog_packet.h"

int      syslog_format(const syslog_msg_t *msg, char *buf, uint16_t buf_size);
uint8_t  syslog_pri(syslog_facility_t facility, syslog_severity_t severity);
syslog_err_t syslog_parse_pri(const char *buf, uint16_t len,
                               uint8_t *pri, uint16_t *offset);
```

## How to test

```bash
cd wheels/protocols/syslog_packet/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **21 C cases** -- PRI calculation (4), format with all fields,
  NILVALUE fields, empty-string NILVALUE, timestamp epoch-zero, known-date
  timestamp, buffer overflow, NULL params, long message truncation, exact-fit
  boundary, PRI parse (basic/zero/max/overflow/malformed/trailing), PRI
  roundtrip, different facilities

## Dependencies

None.

## Origin

Proactive build for remote logging in embedded systems (2026-05-01).

## Configuration macros

None.

## Known issues

None.

## Supported platforms

- [x] Linux host (sim) -- passed 2026-05-01

## API review

- Date: 2026-05-01
- Key decisions: formatter only (no transport), NILVALUE for missing fields,
  ISO 8601 UTC timestamps, STRUCTURED-DATA always NILVALUE

## API convention deviations

None.

## Responsible human

@user
