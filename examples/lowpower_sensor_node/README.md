# Low-Power Sensor Node -- Integration Recipe

Demonstrates six wheels working together as a battery-powered IoT
sensor node that duty-cycles between short wake bursts and long
sleeps. Runs entirely on the host with mock I2C -- no hardware
required.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L1 | `max17048`     | LiPo fuel gauge: cell voltage + state of charge |
| L1 | `ds3231`       | Precision RTC -- periodic wake source |
| L1 | `adxl345`      | 3-axis accelerometer -- motion wake source |
| L1 | `bme280`       | Temperature / humidity / pressure sampler |
| L3 | `boot_counter` | Persistent wake count (proof every wake is unique) |
| L3 | `power_manager`| Cooperative sleep-level voter orchestration |

## What It Demonstrates

1. **Duty cycling** -- The wake loop alternates between RTC ticks and
   motion events. Between events the node is treated as fully asleep;
   only the mock register state advances.

2. **Multi-source wake** -- `classify_wake()` decides whether the
   current wake is RTC-periodic, motion-triggered, or an emergency
   low-battery upload. Different sources can drive different telemetry
   priorities.

3. **Battery threshold escalation** -- Once `max17048` reports SOC
   below `LOW_BATTERY_PCT`, subsequent wakes are reclassified as
   emergency and engage the `radio` voter, blocking deep sleep so the
   node can upload before the cell dies.

4. **Cooperative sleep voting** -- Two `power_manager` voters
   (`sampler`, `radio`) register with different `max_sleep` levels.
   `poll()` returns the most restrictive recommendation. With nothing
   active, `POWER_LEVEL_SHUTDOWN` is permitted.

5. **Persistence across reboots** -- `boot_counter` saves the wake
   count to a mock NVM region. After 10 wakes, a freshly constructed
   `boot_counter_t` loads the same count, proving each wake recorded
   itself uniquely.

## Build and Run

```bash
cd examples/lowpower_sensor_node
cmake -B build -S . && cmake --build build && ./build/lowpower_sensor_node
```

Expected output: 6 integration tests pass. The wake-cycle table prints
10 rows with mixed RTC / motion / EMERG sources, ending with at least
one motion wake and one emergency wake.

## Wake Sequence (Expected Output Snippet)

```
  c | src    | bc# | t (ms) | SOC%  | T  | motion |
  --|--------|-----|--------|-------|----|--------|
  0 | RTC    |   1 |  30000 |  83.0 |  8 |  0.05g |
  1 | motion |   2 |  60000 |  71.0 |  8 |  1.20g |
  2 | RTC    |   3 |  90000 |  59.0 |  9 |  0.05g |
  3 | RTC    |   4 | 120000 |  47.0 |  9 |  0.05g |
  4 | motion |   5 | 150000 |  35.0 |  9 |  1.20g |
  5 | RTC    |   6 | 180000 |  23.0 | 10 |  0.05g |
  6 | EMERG  |   7 | 210000 |  11.0 | 10 |  0.05g |
  7 | EMERG  |   8 | 240000 |   5.0 | 11 |  0.05g |
  ...
  totals: rtc=N motion=M emergency=K (sum=10, expected 10)
```

## Data Flow

```
    [RTC tick]        [motion event]      [low battery]
         |                  |                   |
         v                  v                   v
              classify_wake()
                       |
            +----------+---------+
            |                    |
       wake source            wake source
       = RTC/motion           = EMERGENCY
            |                    |
            v                    v
       sample sensors      radio.activity()
       (max17048,            (block deep sleep)
        adxl345,                  |
        bme280,                   v
        ds3231)             upload telemetry
            |                    |
            +----------+---------+
                       |
                       v
              boot_counter.increment()
                       |
                       v
              power_manager.poll() -> deepest allowed level
                       |
                       v
                 [back to sleep]
```
