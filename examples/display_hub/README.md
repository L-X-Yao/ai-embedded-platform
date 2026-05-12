# Display Hub -- Integration Recipe

Demonstrates six L1 display drivers working from a single application.
Covers OLED, character LCD, 7-segment LED, LED matrix, and two color TFTs.
Runs entirely on the host with mock I2C, GPIO, and SPI.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L1 | `sh1106` | 128x64 OLED display (I2C, framebuffer-based) |
| L1 | `hd44780` | 16x2 character LCD via PCF8574 I2C backpack |
| L1 | `tm1637` | 4-digit 7-segment LED display (GPIO bit-bang) |
| L1 | `max7219` | 8x8 LED matrix / 7-segment (SPI) |
| L1 | `lcd_st7789` | 240x240 color TFT (SPI, RGB565) |
| L1 | `ili9341` | 320x240 color TFT (SPI, RGB565) |

## What It Demonstrates

1. **Three HAL types** -- I2C (write_reg/read_regs), GPIO (bit-bang
   clk/dio), and SPI (transfer or xfer) mocked in a single file.

2. **Framebuffer vs streaming** -- SH1106 renders to a local buffer
   then flushes; TFT displays stream pixel data directly over SPI.

3. **HAL sharing** -- SH1106 and HD44780 share the same mock I2C;
   ST7789 and ILI9341 share the same mock SPI. Devices are isolated
   by their context structs.

4. **Protocol diversity** -- TM1637 uses a custom 2-wire bit-bang
   protocol (not I2C), MAX7219 uses 16-bit SPI register writes,
   TFTs use command/data modes via a DC pin.

## Build and Run

```bash
cd examples/display_hub
cmake -GNinja -B build && ninja -C build && ./build/display_hub
```

Expected output: all 8 integration tests pass, with per-display
operation summaries and a full-cycle demonstration.

## Data Flow

```
  Mock I2C ------> sh1106   (framebuffer -> flush)
       |---------> hd44780  (nibble writes via PCF8574)

  Mock GPIO -----> tm1637   (bit-bang CLK/DIO)

  Mock SPI ------> max7219  (16-bit register writes)
       |---------> st7789   (cmd/data via DC pin)
       |---------> ili9341  (cmd/data via DC pin)
```
