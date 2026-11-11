# hdlc

**Status: candidate**
**Layer: L2 (protocols)**

HDLC frame encoding/decoding with byte stuffing and FCS-16 (CRC-16-CCITT
reflected). Provides both one-shot encode and a byte-at-a-time streaming
parser for interrupt-driven UART reception. Frame format:
`[0x7E] [stuffed data + FCS16] [0x7E]`. Pure C99, no heap, no OS.

## Origin

Proactive build (ADR-011). HDLC frame encoding/decoding with byte stuffing and FCS-16 (CRC-16-CCITT


## Why self-written?

existing open-source has no HDLC framing component. HDLC byte stuffing is the foundation
for PPP, many serial protocols, and custom point-to-point links.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

## API

```c
#include "hdlc.h"

/* One-shot codec */
uint16_t   hdlc_fcs16(const uint8_t *data, size_t len);
hdlc_err_t hdlc_encode(const uint8_t *data, size_t data_len,
                        uint8_t *out, size_t out_cap, size_t *out_len);

/* Streaming parser */
hdlc_err_t   hdlc_parser_init(hdlc_parser_t *parser, uint8_t *buf, size_t capacity);
hdlc_state_t hdlc_parser_feed(hdlc_parser_t *parser, uint8_t byte);
hdlc_err_t   hdlc_parser_get_frame(const hdlc_parser_t *parser,
                                    uint8_t *data, size_t data_cap, size_t *data_len);
void         hdlc_parser_reset(hdlc_parser_t *parser);
```

## Usage

```c
/* Encode */
uint8_t frame[128];
size_t frame_len;
hdlc_encode(payload, payload_len, frame, sizeof(frame), &frame_len);
uart_send(frame, frame_len);

/* Decode (byte-at-a-time in UART IRQ) */
hdlc_parser_t parser;
uint8_t work[64];
hdlc_parser_init(&parser, work, sizeof(work));

void uart_irq(uint8_t byte) {
    if (hdlc_parser_feed(&parser, byte) == HDLC_STATE_COMPLETE) {
        uint8_t decoded[64];
        size_t len;
        hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &len);
        hdlc_parser_reset(&parser);
    }
}
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/protocols\hdlc/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_hdlc
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
