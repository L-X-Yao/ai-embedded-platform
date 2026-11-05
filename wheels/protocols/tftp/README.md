# tftp

**Status: candidate**
**Layer: L2 (protocols)**

Stateless TFTP (Trivial File Transfer Protocol) packet builder and parser
per RFC 1350. Handles all five opcodes: RRQ, WRQ, DATA, ACK, and ERROR.
All buffers are caller-provided. Useful for firmware update over UDP on
bare-metal Ethernet stacks. Pure C99, no heap, no OS.

## Origin

Proactive build (ADR-011). t


## Why self-written?

existing open-source has no TFTP codec. This wheel provides the packet-level primitives
for bootloaders and OTA systems that transfer firmware images over TFTP.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

## API

```c
#include "tftp.h"

int  tftp_parse(const uint8_t *buf, size_t len, tftp_packet_t *pkt);
int  tftp_build_rrq(uint8_t *buf, size_t cap, const char *filename, const char *mode);
int  tftp_build_wrq(uint8_t *buf, size_t cap, const char *filename, const char *mode);
int  tftp_build_data(uint8_t *buf, size_t cap,
                     uint16_t block, const uint8_t *data, size_t len);
int  tftp_build_ack(uint8_t *buf, size_t cap, uint16_t block);
int  tftp_build_error(uint8_t *buf, size_t cap, uint16_t code, const char *msg);
bool tftp_is_last_data(const tftp_packet_t *pkt);
```

A filename holds at most `TFTP_MAX_FILENAME - 1` bytes. The builders and
`tftp_parse` return `TFTP_ERR_OVERFLOW` for a longer one, because a shortened
name is a different file.

## Usage

```c
/* Request a file */
uint8_t pkt[256];
int len = tftp_build_rrq(pkt, sizeof(pkt), "firmware.bin", "octet");
udp_send(pkt, len);

/* Receive data blocks */
tftp_packet_t rx;
tftp_parse(rx_buf, rx_len, &rx);
if (rx.opcode == TFTP_OP_DATA) {
    flash_write(offset, rx.u.data.data, rx.u.data.len);
    len = tftp_build_ack(pkt, sizeof(pkt), rx.u.data.block);
    udp_send(pkt, len);
    if (tftp_is_last_data(&rx)) transfer_complete();
}
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/protocols\tftp/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_tftp
```


## Test status

- Unit tests (sim): 34 pass, 0 fail
- libFuzzer: parses raw input, builds frames in exact-size buffers and reads them back
- HIL: N/A

## API convention deviations

None.


## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
