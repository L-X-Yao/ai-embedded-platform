# Power Monitor -- Integration Recipe

Demonstrates eleven wheels working together as a battery power monitoring
system with user input and alerts. Runs entirely on the host with mock
I2C and GPIO.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `bitmap` | Status flags bitfield (charging, low_battery, overcurrent, ...) |
| L0 | `memory_pool` | Fixed-size block allocator for log entries |
| L0 | `timeout` | Non-blocking measurement interval tracking |
| L0 | `endian` | Big-endian I2C register to host byte-order conversion |
| L1 | `max17048` | Fuel gauge -- battery SoC and voltage via I2C |
| L1 | `ina226` | Precision current/voltage/power monitor via I2C |
| L1 | `gpio_button` | Physical button with debounce |
| L2 | `i2c_scan` | I2C bus device discovery (finds MAX17048 + INA226) |
| L3 | `power_manager` | Cooperative sleep/wake state machine with voters |
| L3 | `button_handler` | Multi-button event handler (click, long-press) |
| L3 | `ring_log` | Circular log buffer for event recording |

## What It Demonstrates

1. **I2C bus discovery** -- i2c_scan probes the mock bus and finds
   MAX17048 at 0x36 and INA226 at 0x40, proving the scan + identify
   pattern works without hardware.

2. **Sensor data acquisition** -- MAX17048 reads battery SoC/voltage,
   INA226 reads bus voltage/current/power. Both use mock register arrays
   with realistic values.

3. **Endian conversion** -- I2C registers are big-endian on the wire;
   the endian wheel converts to host byte order for verification, showing
   the pattern used in real firmware.

4. **Status flag management** -- bitmap tracks system state (charging,
   low_battery, overcurrent, alert_active, etc.) as a compact bitfield,
   with set/clear/toggle/find-first operations.

5. **Fixed-size memory allocation** -- memory_pool provides O(1)
   alloc/free of 64-byte blocks without heap, suitable for log entry
   buffers or message queues.

6. **Power state machine** -- power_manager coordinates multiple voters
   (sensor, radio, UI) to recommend the deepest safe sleep level:
   ACTIVE -> IDLE -> LIGHT_SLEEP -> DEEP_SLEEP, with automatic
   timeout-based expiry.

7. **Button input handling** -- gpio_button provides hardware debounce,
   button_handler adds click/double-click/long-press detection with
   callback-driven event delivery.

8. **Event logging** -- ring_log records all system events with
   timestamps, severity levels, and level-based filtering. Events from
   I2C scan, sensor reads, power transitions, and button presses are
   all logged and read back.

## Build and Run

```bash
cd examples/power_monitor
cmake -GNinja -B build && ninja -C build && ./build/power_monitor
```

Expected output: all 8 integration tests pass, with tables of battery
readings, power measurements, status flags, and log entries.

## Data Flow

```
  i2c_scan ----discover----> [0x36: MAX17048, 0x40: INA226]
                                    |                |
                              max17048_read    ina226_read
                                    |                |
                              SoC / voltage    V / I / P
                                    \               /
                              endian_be16toh   endian_read_be16
                                      \           /
                                    bitmap (status flags)
                                         |
  gpio_button --tick--> button_handler --event--> power_manager
                                                       |
                                                  poll() -> level
                                                       |
                              timeout (measurement interval)
                                         |
                                    ring_log (all events)
                                         |
                                  memory_pool (block storage)
```
