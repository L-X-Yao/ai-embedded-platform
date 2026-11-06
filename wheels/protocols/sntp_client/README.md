# sntp_client

**Status: candidate**
**Layer: L2 (protocols)**

SNTP v4 packet codec and clock offset calculation per RFC 4330. Builds
48-byte SNTP request packets, parses server responses, and computes clock
offset and round-trip delay from NTP timestamps. No transport -- caller
handles UDP send/receive. Includes NTP-to-Unix and Unix-to-NTP conversions.
Pure C99, no heap, fully reentrant.

## Origin

Proactive build (ADR-011). s


## Why self-written?

existing open-source has no standalone SNTP codec. This wheel provides the packet-level
primitives needed to synchronize an RTC or soft clock without pulling in
a full network stack.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

## API

```c
#include "sntp_client.h"

int               sntp_client_build_request(uint8_t *buf, size_t buf_size,
                                            const sntp_timestamp_t *tx_time);
sntp_client_err_t sntp_client_parse_response(const uint8_t *data, size_t len,
                                             sntp_response_t *out);
sntp_client_err_t sntp_client_compute(const sntp_response_t *resp,
                                      const sntp_timestamp_t *t1,
                                      const sntp_timestamp_t *t4,
                                      sntp_result_t *out);
sntp_timestamp_t  sntp_client_unix_to_ntp(uint32_t unix_sec, uint16_t ms);
void              sntp_client_ntp_to_unix(const sntp_timestamp_t *ntp,
                                          uint32_t *unix_sec, uint16_t *ms);
```

## Usage

```c
/* Build and send request */
uint8_t req[SNTP_PACKET_SIZE];
sntp_timestamp_t t1 = sntp_client_unix_to_ntp(now_sec, now_ms);
sntp_client_build_request(req, sizeof(req), &t1);
udp_send(req, SNTP_PACKET_SIZE);

/* Parse response and compute offset */
sntp_response_t resp;
sntp_client_parse_response(rx_buf, rx_len, &resp);

sntp_timestamp_t t4 = sntp_client_unix_to_ntp(arrival_sec, arrival_ms);
sntp_result_t result;
sntp_client_compute(&resp, &t1, &t4, &result);
/* result.offset_ms: add to the local clock; result.unix_sec = server time */
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/protocols\sntp_client/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_sntp_client
```


## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.


## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
