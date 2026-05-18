# modbus_ascii

**Status: candidate**
**Layer: L2 (protocols)**

Modbus ASCII frame encoder and decoder. Frame format:
`':' + hex(addr) + hex(func) + hex(data...) + hex(LRC) + CR LF`.
LRC is the two's complement of the sum of binary bytes. Complements the
modbus_rtu (binary) and modbus_tcp (TCP wrapper) wheels.
Pure C99, no heap, no OS.

## Why not VSF native?

VSF has no Modbus protocol support. Modbus ASCII is used for serial
communication in industrial automation where human-readable frames are
preferred over binary RTU encoding.

## API

```c
#include "modbus_ascii.h"

uint8_t            modbus_ascii_lrc(const uint8_t *data, size_t len);
modbus_ascii_err_t modbus_ascii_decode(const char *ascii, size_t len,
                                        modbus_ascii_frame_t *out);
modbus_ascii_err_t modbus_ascii_encode(const modbus_ascii_frame_t *frame,
                                        char *buf, size_t cap, size_t *out_len);
```

## Usage

```c
/* Decode an ASCII frame from serial */
modbus_ascii_frame_t frame;
modbus_ascii_decode(":0103000A0001F1\r\n", 19, &frame);
/* frame.addr=1, frame.func=3, frame.data={0x00,0x0A,0x00,0x01} */

/* Encode a response */
modbus_ascii_frame_t resp = { .addr = 1, .func = 3,
                              .data = {0x02, 0x00, 0x64}, .data_len = 3 };
char buf[128];
size_t len;
modbus_ascii_encode(&resp, buf, sizeof(buf), &len);
uart_send(buf, len);
```

## Dependencies

None. Pure C99.

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Test status

- Unit tests (sim): PASS
- HIL: N/A
