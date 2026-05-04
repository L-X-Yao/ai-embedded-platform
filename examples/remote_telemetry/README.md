# Remote Telemetry — Integration Recipe

Cross-layer integration recipe demonstrating L0 + L2 wheels working
together in a realistic IoT telemetry pipeline.

## Wheels Used

| Layer | Wheel | Role in recipe |
|-------|-------|---------------|
| L0 | `ring_buffer` | Packet retry queue with length-prefixed framing |
| L0 | `crc16` | Response integrity verification (CRC16-MODBUS) |
| L0 | `json_parser` | Verify serialized sensor payloads (SAX parsing) |
| L2 | `mqtt_packet` | Build MQTT 3.1.1 CONNECT and PUBLISH packets |
| L2 | `at_command` | AT command flow for cellular modem interaction |

## Data Flow

```
sensor data
    |
    v
JSON payload (snprintf build + json_parser verify)
    |
    v
MQTT PUBLISH packet (mqtt_packet)
    |
    v
retry queue (ring_buffer with 2-byte length header)
    |
    v
AT+CIPSEND via at_command (mock UART HAL)
    |
    v
CRC16 verify on response (crc16)
```

## What It Demonstrates

- **Packet building**: MQTT CONNECT and PUBLISH with real binary encoding
- **JSON round-trip**: build sensor JSON, then verify it with the SAX parser
- **Retry buffering**: ring_buffer as a packet queue with length-prefixed
  entries, supporting peek (for retransmit) and pop (on ACK)
- **AT modem flow**: send AT commands, wait for prompt, send raw data,
  verify SEND OK -- the exact pattern used with modules like Air780EG,
  SIM800, ESP-AT
- **CRC verification**: append and verify CRC16-MODBUS on response frames,
  both one-shot and streaming APIs
- **Mock HAL**: UART send is a simple callback capturing bytes -- no
  hardware needed, runs on any host

## Build & Run

```bash
# From this directory:
cmake -B build -G Ninja
cmake --build build
./build/remote_telemetry

# Or from WSL:
cd examples/remote_telemetry
cmake -B build -G Ninja && ninja -C build && ./build/remote_telemetry
```

## Expected Output

All 7 steps print their results, ending with:

```
  Result: ALL STEPS PASSED
```

The hex dumps show actual MQTT wire format and the bytes sent through
the mock UART, useful for comparing against a Wireshark capture of real
MQTT traffic.
