# CAN Bus Gateway -- Integration Recipe

Demonstrates four wheels working together as a vehicle-bus gateway:
an MCP2515 SPI CAN controller receives a stream of mixed CAN frames,
dispatched to either an ISO-TP reassembler or a J1939 PGN decoder
based on CAN ID. Runs entirely on the host with a mock SPI register
file -- no hardware required.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L1 | `mcp2515` | SPI CAN bus controller (driver) |
| L2 | `canbus`  | Generic CAN frame validate / pack / signal-extract |
| L2 | `isotp`   | ISO 15765-2 multi-frame reassembly (SF + FF + CF) |
| L2 | `j1939`   | SAE J1939 PGN / priority / source decode |

## What It Demonstrates

1. **Mock SPI controller** -- A 256-byte register file plus an
   instruction state machine emulate enough of the MCP2515 to drive
   the real `mcp2515.c` driver. The driver writes / reads through the
   same SPI HAL it would use against silicon; the mock loads queued
   CAN frames into the RXB0 register set on demand.

2. **Routing by CAN ID** -- `gateway_dispatch_one()` looks at the
   message ID. Extended IDs equal to `UDS_RX_ID` (0x18DA10F1) are fed
   to the ISO-TP reassembler; all other extended IDs are decoded as
   J1939.

3. **Single-frame vs multi-frame** -- The ISO-TP path handles both
   Single Frame (full message in one CAN frame) and First / Consecutive
   Frame sequences. Reassembly completes asynchronously; the gateway
   forwards to its uplink only when `isotp_rx_feed()` returns
   `ISOTP_OK`.

4. **DBC-style signal decoding** -- After J1939 routes a frame to a
   PGN handler, `canbus_signal_extract()` pulls a bit-level signal
   (Intel byte order) out of the 8-byte payload, ready for telemetry.

5. **Stateless validation** -- Every frame is run through
   `canbus_frame_validate()` first, so malformed inputs cannot leak
   into the downstream decoders.

## Build and Run

```bash
cd examples/can_gateway
cmake -B build -S . && cmake --build build && ./build/can_gateway
```

Expected output: 7 integration tests pass. The mixed-traffic test
prints the count of dispatched frames and a summary of the resulting
uplink items.

## Frame Mix (Test Scenarios)

| Test | Frames Enqueued | Decoded |
|------|------------------|---------|
| `test_dispatch_j1939_broadcasts` | 5 single-frame J1939 (EEC1 + speed mix) | 5 PGN uplink items |
| `test_dispatch_isotp_single_frame` | 1 ISO-TP SF (5-byte payload)             | 1 reassembled message |
| `test_dispatch_isotp_multi_frame`  | 1 FF + 3 CF (25-byte payload)            | 1 reassembled message |
| `test_mixed_traffic`               | 2 PGN + (FF + 2 CF) + 1 PGN              | 2 PGN + 1 ISO-TP + 1 PGN |

## Data Flow

```
                       mock SPI register file
                                |
                                v
                          mcp2515_receive()  <-- driver SPI calls
                                |
                                v  (raw mcp2515_msg_t)
                       gateway_dispatch_one()
                                |
                  +-------------+--------------+
                  |                            |
        id == UDS_RX_ID                   extended id
                  |                            |
                  v                            v
            isotp_rx_feed()             j1939_decode_id()
                  |                            |
                  v                            v
              (SF or                     uplink_push_pgn()
               last CF)                       |
                  |                           v
                  v                     [TX to uplink:
            uplink_push_isotp()          pgn + priority +
                  |                       source + bytes]
                  v
            [TX to uplink:
             reassembled payload]
```
