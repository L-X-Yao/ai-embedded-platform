# Air Quality Monitor -- Integration Recipe

Demonstrates six L1 environmental sensor drivers working together with
cross-sensor validation and AQI computation. Runs on host with mock I2C.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L1 | `ccs811` | eCO2 + TVOC measurement |
| L1 | `sgp40` | VOC raw signal |
| L1 | `bme680` | Temperature + pressure + humidity + gas |
| L1 | `sht40` | Precision temperature + humidity |
| L1 | `aht20` | Budget temperature + humidity |
| L1 | `veml7700` | Ambient light level (lux) |

## Build and Run

```bash
cd examples/air_quality
cmake -GNinja -B build && ninja -C build && ./build/air_quality
```

## Data Flow

```
  ccs811 (eCO2/TVOC) ──┐
  sgp40 (VOC raw)    ──┤
  bme680 (T/P/H/gas) ──┼──> cross-validate temps ──> AQI computation
  sht40 (T/H)        ──┤
  aht20 (T/H)        ──┤
  veml7700 (lux)      ──┘
```
