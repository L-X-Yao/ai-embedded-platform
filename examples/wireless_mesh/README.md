# Wireless Sensor Mesh -- Integration Recipe

Demonstrates six wheels working together as a wireless sensor mesh
network with dual-radio capability. Runs entirely on the host with
mock SPI -- no hardware required.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `slip` | SLIP framing for inter-node serial communication |
| L0 | `checksum` | Packet integrity verification (XOR, Fletcher-16) |
| L0 | `rle_codec` | Compress sensor payloads before transmission |
| L1 | `nrf24l01` | 2.4 GHz short-range radio for mesh hops |
| L1 | `lora_sx1276` | LoRa long-range radio for gateway uplink |
| L2 | `lorawan_packet` | LoRaWAN frame encoding for cloud delivery |

## What It Demonstrates

1. **Dual SPI mock injection** -- Both radio drivers receive independent
   mock SPI callbacks with full register-file simulation and FIFO
   loopback, proving the DI pattern scales to multiple peripherals.

2. **Codec pipeline** -- Sensor data flows through RLE compression,
   checksum attachment, and SLIP framing before radio transmission,
   showing how L0 primitives compose into a data pipeline.

3. **Radio abstraction** -- The same framed payload can be sent via
   nRF24L01+ (short range, mesh hop) or SX1276 (long range, gateway),
   demonstrating radio-agnostic upper layers.

4. **Protocol layering** -- After decompression, data is wrapped in a
   LoRaWAN uplink frame for cloud delivery, showing L2 protocol
   encoding on top of L1 transport.

5. **Round-trip verification** -- Every stage is verified: compress,
   frame, transmit, receive, deframe, verify checksum, decompress,
   and compare against the original sensor data.

## Build and Run

```bash
cd examples/wireless_mesh
cmake -GNinja -B build && ninja -C build && ./build/wireless_mesh
```

Expected output: all 8 integration tests pass, with detailed output
showing compression ratios, packet sizes, and end-to-end verification.

## Data Flow

```
  Sensor Data (32 bytes)
       |
       v
  [RLE compress] -----> compressed payload (~10 bytes)
       |
       v
  [checksum] ---------> + Fletcher-16 trailer
       |
       v
  [SLIP frame] -------> + END delimiters + escape sequences
       |
       +----------+------------------+
       |          |                  |
       v          v                  v
    nRF24L01   SX1276           (serial link)
   (mesh hop) (gateway)
       |          |
       v          v
  [SLIP decode] <---- received data
       |
       v
  [checksum verify] -- integrity check
       |
       v
  [RLE decompress] --> original sensor data
       |
       v
  [lorawan_packet] --> LoRaWAN uplink frame --> cloud
```
