# Smart Home IoT Node -- Integration Recipe

Demonstrates nine wheels working together as a smart home IoT sensor
node. Runs entirely on the host with mock I2C and mock PWM HAL.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `debounce` | Button input debouncing (noise rejection) |
| L0 | `timer_wheel` | One-shot scheduled alert callbacks |
| L0 | `led_pattern` | Heartbeat blink pattern engine |
| L1 | `bme280` | Temperature + humidity + pressure sensor |
| L1 | `bh1750` | Ambient light level sensor |
| L1 | `ws2812` | RGB LED strip driver (PWM bit-bang) |
| L2 | `cbor_lite` | Binary CBOR payload encoding for telemetry |
| L3 | `scheduler` | Periodic task scheduler (sensor polling) |
| L3 | `event_bus` | Pub/sub event dispatch (decouples producers/consumers) |

## What It Demonstrates

1. **Mock HAL injection** -- BME280 and BH1750 receive mock I2C
   callbacks; WS2812 receives a mock PWM HAL. All run without hardware.

2. **Scheduler-driven sensing** -- `scheduler` runs periodic sensor
   reads. Each callback reads BME280 and BH1750, packages the data, and
   publishes to `event_bus`.

3. **Event-driven architecture** -- Sensor data flows through
   `event_bus`. A subscriber encodes the payload as CBOR via
   `cbor_lite`, ready for transmission.

4. **Reactive LED control** -- `led_pattern` generates a heartbeat
   blink pattern that drives `ws2812` color data (bright when ON,
   dim when OFF).

5. **Button debouncing** -- `debounce` filters noisy GPIO input,
   detecting clean rising edges for button press events published
   to `event_bus`.

6. **One-shot timers** -- `timer_wheel` schedules a deferred alert
   callback, demonstrating timeout-based actions.

## Build and Run

```bash
cd examples/smart_home
cmake -GNinja -B build && ninja -C build && ./build/smart_home
```

Expected output: all 9 integration tests pass, with sensor readings,
CBOR encoding verification, and event flow confirmation printed.

## Data Flow

```
  bme280 ----[read]----> scheduler callback ---[publish]--> event_bus
  bh1750 ----[read]---->       |                                |
                               |                                v
                          (every N ticks)              on_sensor_data
                                                           |
                button GPIO                                v
                    |                                cbor_lite encode
                    v                                      |
                debounce                                   v
                    |                              CBOR payload ready
                    v                              (for MQTT / LoRa / ...)
            event_bus "button"

  led_pattern --[tick]--> on/off state ---> ws2812_send(bright/dim)

  timer_wheel --[tick]--> one-shot alert callback
```
