# uds

**Status: candidate**
**Layer: L2 (protocols)**

Unified Diagnostic Services (ISO 14229) message codec. Encodes and decodes
UDS request and response messages including DiagnosticSessionControl,
ECUReset, ReadDataByIdentifier, WriteDataByIdentifier, TesterPresent, and
NegativeResponse. Transport-independent -- pair with ISO-TP for CAN.
Pure C99, no heap, no OS.

## Origin

Proactive build (ADR-011). Unified Diagnostic Services (ISO 14229) message codec. Encodes and decodes


## Why self-written?

existing open-source has no UDS support. UDS is the standard diagnostic protocol for
automotive ECUs, required for production diagnostics and EOL testing.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99. Typically paired with the isotp wheel for CAN transport.

## API

```c
#include "uds.h"

uds_err_t uds_decode(const uint8_t *buf, size_t len, uds_msg_t *out);
uds_err_t uds_encode_request(uint8_t sid, const uint8_t *data, size_t data_len,
                              uint8_t *buf, size_t cap, size_t *out_len);
uds_err_t uds_encode_response(uint8_t sid, const uint8_t *data, size_t data_len,
                               uint8_t *buf, size_t cap, size_t *out_len);
uds_err_t uds_encode_negative(uint8_t rejected_sid, uint8_t nrc,
                               uint8_t *buf, size_t cap, size_t *out_len);
bool      uds_is_positive_response(uint8_t sid_byte);
```

## Usage

```c
/* Send ReadDataByIdentifier request */
uint8_t did[] = {0xF1, 0x90};  /* VIN DID */
uint8_t buf[64];
size_t len;
uds_encode_request(UDS_SID_READ_DATA_BY_ID, did, 2, buf, sizeof(buf), &len);
isotp_send(buf, len);

/* Decode response */
uds_msg_t msg;
uds_decode(rx_data, rx_len, &msg);
if (msg.is_negative) {
    handle_nrc(msg.nrc);
} else {
    /* msg.data contains the DID value */
}
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/protocols\uds/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_uds
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

@L-X-Yao
