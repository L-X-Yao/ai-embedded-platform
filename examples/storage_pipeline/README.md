# Storage Pipeline -- Integration Recipe

Demonstrates seven wheels working together as a complete sensor-to-persistent-storage
data pipeline. Runs entirely on the host with mocked flash and I2C.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `fal` | Flash abstraction layer (RAM-backed mock) |
| L0 | `kvs` (FlashDB) | Persistent key-value store on fal |
| L0 | `fifo_queue` | Fixed-size FIFO for buffering sensor samples |
| L0 | `byte_buffer` | Linear buffer for binary packet assembly |
| L0 | `string_utils` | String formatting, parsing, join/split |
| L0 | `hash_fnv` | FNV-1a hash for unique key generation |
| L1 | `at24cxx` | I2C EEPROM driver for calibration storage |

## What It Demonstrates

1. **Mock HAL injection** -- Flash operations via fal use a RAM array;
   at24cxx receives mock I2C callbacks. No hardware required.

2. **Multi-stage pipeline** -- Sensor data flows through buffering (fifo_queue),
   binary assembly (byte_buffer), string formatting (string_utils),
   key hashing (hash_fnv), and persistent storage (kvs on fal).

3. **Dual storage paths** -- Bulk sensor data goes to kvs (flash-backed);
   calibration data goes to at24cxx (EEPROM).

4. **Round-trip verification** -- Every stored value is read back, parsed,
   and compared to the original to prove data integrity.

## Build and Run

```bash
cd examples/storage_pipeline
cmake -GNinja -B build && ninja -C build && ./build/storage_pipeline
```

Expected output: all 9 integration tests pass, with a table of stored
sensor samples and verified read-back.

## Data Flow

```
  sensor samples
       |
       v
  fifo_queue (buffering, fixed-size FIFO)
       |
       v
  byte_buffer (binary packet assembly)
       |
       +---> hash_fnv (key generation: "s42_t2000" -> "d_a1b2c3d4")
       |
       +---> string_utils (value formatting: "25000|2000|42")
       |
       v
  kvs / FlashDB (persistent key-value store)
       |
       v
  fal (flash abstraction layer, RAM-backed mock)

  at24cxx (EEPROM) <--- calibration data (separate path)
```
