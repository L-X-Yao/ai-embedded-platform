# MCU Compatibility Matrix

All 115 L1 drivers use HAL injection -- they compile on **any MCU with a C99 compiler**. This matrix shows which bus interface each driver requires, so you can quickly find components that work with your hardware.

## Quick reference: what bus does my MCU need?

| Bus | Typical MCU support | Driver count |
|-----|-------------------|-------------|
| I2C | All modern MCUs (STM32, GD32, ESP32, nRF52, RP2040, RISC-V) | 77 |
| SPI | All modern MCUs | 20 |
| GPIO | All MCUs | 16 |
| OneWire | Any MCU with GPIO (bit-banged) | 1 |

> **No `#ifdef` per platform.** You inject your HAL callbacks at init time. The same driver binary runs on Cortex-M0, Cortex-M7, Xtensa, RISC-V, or x86 desktop (mock HAL for testing).

---

## I2C drivers (77)

### Sensors -- Environmental

| Driver | Chip | Measures | Extras |
|--------|------|----------|--------|
| `aht20` | AHT20 | temperature, humidity | CRC validation |
| `bme280` | Bosch BME280 | temperature, humidity, pressure | Bosch compensation algorithm |
| `bme680` | Bosch BME680 | temperature, humidity, pressure, gas | VOC/IAQ measurement |
| `bme688` | Bosch BME688 | temperature, humidity, pressure, gas | improved gas sensing |
| `bmp280` | Bosch BMP280 | pressure, temperature | barometric altitude |
| `bmp388` | Bosch BMP388 | pressure, temperature | high precision barometer |
| `dps310` | Infineon DPS310 | pressure, temperature | |
| `lps22hb` | ST LPS22HB | pressure, temperature | |
| `ms5611` | TE MS5611 | pressure, temperature | PROM calibration |
| `sht30` | Sensirion SHT30 | temperature, humidity | |
| `sht40` | Sensirion SHT40 | temperature, humidity | high precision |
| `shtc3` | Sensirion SHTC3 | temperature, humidity | low-power sleep mode |
| `si7021` | Silicon Labs Si7021 | temperature, humidity | no-hold mode |
| `htu21d` | TE HTU21D | temperature, humidity | CRC-8 validation |
| `xgzp6816d` | CFSensor XGZP6816D | differential pressure | |

### Sensors -- Motion / IMU

| Driver | Chip | Axes | Extras |
|--------|------|------|--------|
| `adxl345` | ADI ADXL345 | 3-axis accel | motion interrupt |
| `bma400` | Bosch BMA400 | 3-axis accel | ultra-low-power |
| `bmi160` | Bosch BMI160 | 6-axis (accel + gyro) | step counter |
| `bmi270` | Bosch BMI270 | 6-axis (accel + gyro) | config file loading (SPI) |
| `bno055` | Bosch BNO055 | 9-axis (accel + gyro + mag) | onboard sensor fusion |
| `icm42688` | TDK ICM-42688 | 6-axis (accel + gyro) | high performance |
| `lis2dh12` | ST LIS2DH12 | 3-axis accel | ultra-low-power |
| `lis3dh` | ST LIS3DH | 3-axis accel | FIFO buffer |
| `lsm6dso` | ST LSM6DSO | 6-axis (accel + gyro) | |
| `mpu6050` | InvenSense MPU6050 | 6-axis (accel + gyro) | |
| `qmi8658a` | QST QMI8658A | 6-axis (accel + gyro) | temperature compensation |

### Sensors -- Magnetic / Position

| Driver | Chip | Axes | Extras |
|--------|------|------|--------|
| `as5600` | ams AS5600 | 12-bit rotary | magnetic position sensor |
| `bmm150` | Bosch BMM150 | 3-axis mag | compensation algorithm |
| `hmc5883l` | Honeywell HMC5883L | 3-axis mag | offset correction |
| `lis3mdl` | ST LIS3MDL | 3-axis mag | configurable range |
| `qmc5883p` | QST QMC5883P | 3-axis mag | calibration support |

### Sensors -- Light / Color / UV

| Driver | Chip | Measures | Extras |
|--------|------|----------|--------|
| `apds9960` | Broadcom APDS-9960 | proximity, gesture, RGB, ambient | gesture recognition |
| `bh1750` | ROHM BH1750 | ambient light (lux) | multiple resolutions |
| `ltr390` | Lite-On LTR-390 | UV index, ambient light | |
| `opt3001` | TI OPT3001 | ambient light (lux) | |
| `tcs34725` | ams TCS34725 | RGB color | clear channel |
| `tsl2591` | ams TSL2591 | ambient light | high dynamic range |
| `veml7700` | Vishay VEML7700 | ambient light (lux) | |

### Sensors -- Temperature (dedicated)

| Driver | Chip | Resolution | Extras |
|--------|------|-----------|--------|
| `lm75` | NXP LM75 | 9-12 bit | |
| `mlx90614` | Melexis MLX90614 | non-contact IR | SMBus |
| `pct2075` | NXP PCT2075 | 11 bit | LM75 successor |
| `tmp102` | TI TMP102 | 12 bit | |
| `tmp117` | TI TMP117 | 16 bit | high accuracy |

### Sensors -- Power / Current

| Driver | Chip | Channels | Extras |
|--------|------|---------|--------|
| `bq27441` | TI BQ27441 | 1-cell LiPo | fuel gauge |
| `ina219` | TI INA219 | 1 channel | current, voltage, power |
| `ina226` | TI INA226 | 1 channel | high precision |
| `ina3221` | TI INA3221 | 3 channels | |
| `max17048` | Maxim MAX17048 | 1-cell LiPo | fuel gauge |
| `pac1952` | Microchip PAC1952 | 2 channels | energy accumulator |

### Sensors -- ADC

| Driver | Chip | Resolution | Channels |
|--------|------|-----------|----------|
| `ads1015` | TI ADS1015 | 12-bit | 4 (muxed) |
| `ads1115` | TI ADS1115 | 16-bit | 4 (muxed) |

### Sensors -- Distance

| Driver | Chip | Range | Technology |
|--------|------|-------|-----------|
| `vl53l0x` | ST VL53L0X | up to 2m | Time-of-Flight laser |
| `vl53l1x` | ST VL53L1X | up to 4m | Time-of-Flight laser |

### Sensors -- Gas / Air Quality

| Driver | Chip | Measures |
|--------|------|----------|
| `ccs811` | ams CCS811 | eCO2, TVOC |
| `scd40` | Sensirion SCD40 | CO2 (NDIR) |
| `sgp30` | Sensirion SGP30 | eCO2, TVOC |
| `sgp40` | Sensirion SGP40 | VOC raw signal |

### Displays -- I2C

| Driver | Chip | Type | Resolution |
|--------|------|------|-----------|
| `hd44780` | HD44780 via PCF8574 | character LCD | 16x2 / 20x4 |
| `oled_ssd1306` | Solomon SSD1306 | OLED | 128x64 |
| `sh1106` | Sino Wealth SH1106 | OLED | 132x64 |

### Display Controllers

| Driver | Chip | Type |
|--------|------|------|
| `ht16k33` | Holtek HT16K33 | 16x8 LED matrix/segment |
| `is31fl3731` | ISSI IS31FL3731 | 9x16 LED matrix |

### I/O Expanders / Mux

| Driver | Chip | Channels | Extras |
|--------|------|---------|--------|
| `aw9523` | AWINIC AW9523 | 16 GPIO | LED PWM support |
| `mcp23017` | Microchip MCP23017 | 16 GPIO | interrupt outputs |
| `pcf8574` | NXP PCF8574 | 8 GPIO | |
| `tca9548a` | TI TCA9548A | 8 I2C channels | I2C multiplexer |

### RTC (Real-Time Clock)

| Driver | Chip | Extras |
|--------|------|--------|
| `ds3231` | Maxim DS3231 | high precision, 32kHz output |
| `pcf8563` | NXP PCF8563 | alarm, timer |

### Input -- Touch

| Driver | Chip | Touches | Extras |
|--------|------|--------|--------|
| `cst816s` | Hynitron CST816S | single | gesture recognition |
| `ft6x36` | FocalTech FT6X36 | 1-2 | |
| `mpr121` | NXP MPR121 | 12-ch capacitive | per-channel thresholds |

### Actuators -- I2C

| Driver | Chip | Type |
|--------|------|------|
| `drv2605` | TI DRV2605 | haptic motor driver |
| `mcp4725` | Microchip MCP4725 | 12-bit DAC |
| `mcp4728` | Microchip MCP4728 | 4-ch 12-bit DAC |
| `pca9685` | NXP PCA9685 | 16-ch PWM servo driver |

### Sensors -- Pulse / Bio

| Driver | Chip | Measures |
|--------|------|----------|
| `max30102` | Maxim MAX30102 | SpO2, heart rate |

### Storage -- I2C

| Driver | Chip | Type | Extras |
|--------|------|------|--------|
| `at24cxx` | Atmel AT24Cxx | I2C EEPROM | 1KB-256KB, page-boundary splitting |

---

## SPI drivers (20)

### Displays -- SPI

| Driver | Chip | Type | Resolution |
|--------|------|------|-----------|
| `gc9a01` | GalaxyCore GC9A01 | round TFT | 240x240 |
| `ili9341` | Ilitek ILI9341 | TFT | 240x320 |
| `lcd_st7789` | Sitronix ST7789 | TFT | 240x320 |
| `st7735` | Sitronix ST7735 | TFT | 128x160 |
| `max7219` | Maxim MAX7219 | LED matrix / 7-seg | 8x8 per chip |

### Communication -- SPI

| Driver | Chip | Protocol | Extras |
|--------|------|---------|--------|
| `cc1101` | TI CC1101 | sub-GHz radio | status byte feedback |
| `enc28j60` | Microchip ENC28J60 | Ethernet 10Base-T | MAC + PHY |
| `lora_sx1276` | Semtech SX1276 | LoRa | NRESET control |
| `mcp2515` | Microchip MCP2515 | CAN 2.0B | FIFO management |
| `nrf24l01` | Nordic nRF24L01+ | 2.4GHz proprietary | up to 2Mbps |
| `sx1262` | Semtech SX1262 | LoRa | busy polling |
| `w5500` | WIZnet W5500 | TCP/IP Ethernet | hardware stack |

### Storage -- SPI

| Driver | Chip | Type | Extras |
|--------|------|------|--------|
| `sd_spi` | SD card | block storage | CMD/ACMD protocol |
| `w25qxx` | Winbond W25Qxx | NOR flash | sector/block erase |

### Sensors -- SPI

| Driver | Chip | Measures | Extras |
|--------|------|----------|--------|
| `mcp3008` | Microchip MCP3008 | 10-bit ADC, 8ch | |
| `max31855` | Maxim MAX31855 | K-type thermocouple | |
| `max31865` | Maxim MAX31865 | PT100/PT1000 RTD | |
| `max6675` | Maxim MAX6675 | K-type thermocouple | 12-bit |

### LEDs -- SPI

| Driver | Chip | Type |
|--------|------|------|
| `apa102` | APA102 | addressable RGB LED |

### RFID / NFC -- SPI

| Driver | Chip | Protocol | Extras |
|--------|------|---------|--------|
| `rc522` | NXP MFRC522 | MIFARE | ISO/IEC 14443-A |

---

## GPIO drivers (16)

### Input

| Driver | Type | Extras |
|--------|------|--------|
| `gpio_button` | push button | debounce, long-press, multi-click |
| `keypad_matrix` | matrix keypad | row/column scanning |
| `rotary_encoder` | quadrature encoder | click detection |

### Output / Indication

| Driver | Type | Extras |
|--------|------|--------|
| `gpio_led` | single LED | blink, breathe patterns |
| `buzzer` | passive buzzer | frequency + duty (GPIO+PWM) |

### Sensors -- GPIO

| Driver | Chip | Measures | Protocol |
|--------|------|----------|---------|
| `dht11` | DHT11 | temperature, humidity | single-wire pulse |
| `hcsr04` | HC-SR04 | distance (2cm-4m) | trigger/echo |
| `hx711` | HX711 | weight (24-bit ADC) | clock/data |

### Motor / Actuator -- GPIO

| Driver | Chip | Type | Extras |
|--------|------|------|--------|
| `a4988` | Allegro A4988 | stepper driver | step/direction |
| `drv8833` | TI DRV8833 | dual H-bridge | GPIO+PWM |
| `stepper` | generic | stepper motor | trapezoidal acceleration |
| `tb6612` | Toshiba TB6612 | dual H-bridge | GPIO+PWM |
| `servo` | generic | servo motor | PWM pulse |

### LEDs -- GPIO (bit-banged)

| Driver | Chip | Type | Extras |
|--------|------|------|--------|
| `sk6812` | SK6812 | RGBW addressable LED | 800kHz protocol |
| `ws2812` | WS2812 | RGB addressable LED | 800kHz protocol |
| `tm1637` | Titan TM1637 | 4-digit 7-segment | 2-wire GPIO |

---

## OneWire drivers (1)

| Driver | Chip | Measures | Extras |
|--------|------|----------|--------|
| `ds18b20` | Maxim DS18B20 | temperature | configurable 9-12 bit resolution |

> Use with `soft_onewire` (L0 primitive) for bit-banged OneWire on any GPIO pin.

---

## Communication modules (standalone)

| Driver | Chip | Protocol | Bus |
|--------|------|---------|-----|
| `air780eg` | Quectel Air780EG | 4G LTE + GNSS | custom transport |

---

## By use case

### "I'm building a weather station"
`bme280` (temp/hum/pressure) + `bh1750` (light) + `oled_ssd1306` (display) + `sd_spi` (logging)

### "I'm building a drone / robot"
`icm42688` or `mpu6050` (IMU) + `qmc5883p` (magnetometer) + `bmp388` (barometer) + `mahony_ahrs` (L0, sensor fusion)

### "I'm building an IoT device"
`bme280` (sensors) + `mqtt_packet` + `mqtt_client` (L2/L3, protocol) + `lora_sx1276` or `w5500` (connectivity)

### "I'm building a motor controller"
`a4988` or `stepper` (stepper) + `drv8833` or `tb6612` (DC motor) + `pid_controller` (L0, control loop) + `rotary_encoder` (feedback)

### "I'm building a display device"
`oled_ssd1306` or `sh1106` (OLED) + `lcd_st7789` or `ili9341` (TFT) + `framebuffer` + `font_5x7` (L0, rendering)
