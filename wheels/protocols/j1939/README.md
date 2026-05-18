# j1939

**Status: candidate**
**Layer: L2 (protocols)**

SAE J1939 CAN identifier and PGN (Parameter Group Number) codec. Encodes
and decodes 29-bit extended CAN identifiers into priority, PGN, source
address, and destination address fields. Distinguishes PDU1 (peer-to-peer,
PF < 240) from PDU2 (broadcast, PF >= 240). Pure C99, no heap, no OS.

## Why not VSF native?

VSF has no J1939 support. J1939 is the standard CAN application layer for
heavy-duty vehicles, agricultural equipment, and industrial machinery.

## API

```c
#include "j1939.h"

j1939_err_t j1939_decode_id(uint32_t can_id, j1939_id_t *out);
j1939_err_t j1939_encode_id(const j1939_id_t *fields, uint32_t *can_id);
uint32_t    j1939_pgn_from_id(uint32_t can_id);
bool        j1939_is_pdu1(uint32_t pgn);
```

## Usage

```c
/* Decode a received CAN ID */
j1939_id_t id;
j1939_decode_id(0x18FEF200, &id);
/* id.priority=6, id.pgn=0xFEF2 (Fuel Economy), id.source=0x00 */

/* Encode a CAN ID for transmission */
j1939_id_t tx = { .priority = 3, .pgn = 0xFF00, .source = 0x80 };
uint32_t can_id;
j1939_encode_id(&tx, &can_id);
can_send(can_id, data, 8);
```

## Dependencies

None. Pure C99.

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Test status

- Unit tests (sim): PASS
- HIL: N/A
