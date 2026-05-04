# Environmental Monitor -- Integration Recipe

Demonstrates five wheels working together as a complete environmental
monitoring application. Runs entirely on the host with mock I2C.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `ring_buffer` | Backing store for logged CSV records |
| L1 | `bmp280` | Temperature + barometric pressure sensor driver |
| L1 | `sht30` | Temperature + humidity sensor driver |
| L3 | `sensor_hub` | Unified polling, statistics (min/max/avg) |
| L3 | `data_logger` | CSV record formatting and storage |

## What It Demonstrates

1. **Mock HAL injection** -- Each driver receives mock I2C callbacks,
   proving the dependency-injection pattern works without hardware.

2. **Adapter pattern** -- Small adapter functions bridge each driver's
   read API to sensor_hub's uniform `(*read)(ctx, &value)` callback.

3. **Wheel composition** -- data_logger writes to ring_buffer via a
   storage callback, showing how L3 modules compose with L0 primitives.

4. **End-to-end data flow** -- Sensors are polled, values pass through
   sensor_hub, get logged as CSV into ring_buffer, and are read back
   and verified.

## Build and Run

```bash
cd examples/env_monitor
cmake -GNinja -B build && ninja -C build && ./build/env_monitor
```

Expected output: all 7 integration tests pass, with a table of
simulated environmental readings and CSV data printed.

## Data Flow

```
  bmp280 ----[adapter]--> sensor_hub --poll--> application
  sht30  ----[adapter]-->     |                    |
                              |                    v
                           statistics       data_logger
                                                   |
                                                   v
                                             ring_buffer
                                                   |
                                                   v
                                            CSV read-back
```
