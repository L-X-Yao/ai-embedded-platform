# wheels/drivers/ — L1 硬件驱动

具体芯片/传感器/外设的驱动，通过 HAL 函数指针注入跨 MCU。

**命名**：按芯片/器件型号，不按泛称。
- `mpu6050/`（六轴传感器）
- `w25qxx/`（SPI flash 家族）
- `display/`（泛名，不同芯片差异大）

**合格门槛**（L1）：
- sim 里有 mock 测试（模拟硬件行为）
- 至少 1 次真机验证记录（写入 README 的"支持的平台"节，含日期）
- 所有硬件访问必须走 HAL 函数指针注入，**禁止**直接操作寄存器

详细分类（按总线/用途）见 [MCU Compatibility Matrix](../../docs/mcu-compatibility.md)。

## Origin

Proactive build (ADR-011). w


## Why self-written?

Searched GitHub (ADR-010 compliance). No suitable existing implementation found. Self-written for platform convention consistency.


## Known issues

None.


## How to test

```bash
cd wheels/drivers/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_drivers
```


## API convention deviations

None.


## 目录清单（115 wheels）

| Wheel | 芯片 | 接口 | 状态 |
|-------|------|------|------|
| `a4988` | Allegro A4988 步进驱动 | GPIO | candidate |
| `ads1015` | TI ADS1015 12-bit ADC | I2C | candidate |
| `ads1115` | TI ADS1115 16-bit ADC | I2C | candidate |
| `adxl345` | ADI ADXL345 三轴加速度计 | I2C | candidate |
| `aht20` | AHT20 温湿度 | I2C | candidate |
| `air780eg` | Quectel Air780EG 4G/GNSS | custom | candidate |
| `apa102` | APA102 SPI 可寻址 RGB LED | SPI | candidate |
| `apds9960` | Broadcom APDS-9960 手势/颜色/接近 | I2C | candidate |
| `as5600` | ams AS5600 磁编码器 | I2C | candidate |
| `at24cxx` | Atmel AT24C EEPROM | I2C | candidate |
| `aw9523` | AWINIC AW9523 16-bit IO+LED | I2C | candidate |
| `bh1750` | ROHM BH1750 光照传感器 | I2C | candidate |
| `bma400` | Bosch BMA400 超低功耗加速度计 | I2C | candidate |
| `bme280` | Bosch BME280 温湿压 | I2C | candidate |
| `bme680` | Bosch BME680 环境+气体 | I2C | candidate |
| `bme688` | Bosch BME688 环境+气体 | I2C | candidate |
| `bmi160` | Bosch BMI160 六轴 IMU | I2C | candidate |
| `bmi270` | Bosch BMI270 六轴 IMU | SPI | candidate |
| `bmm150` | Bosch BMM150 三轴磁力计 | I2C | candidate |
| `bmp280` | Bosch BMP280 气压/温度 | I2C | candidate |
| `bmp388` | Bosch BMP388 高精度气压 | I2C | candidate |
| `bno055` | Bosch BNO055 九轴+融合 | I2C | candidate |
| `bq27441` | TI BQ27441 电量计 | I2C | candidate |
| `buzzer` | 无源蜂鸣器 | PWM | candidate |
| `cc1101` | TI CC1101 Sub-GHz 收发器 | SPI | candidate |
| `ccs811` | ams CCS811 eCO2/TVOC | I2C | candidate |
| `cst816s` | Hynitron CST816S 触摸 | I2C | candidate |
| `dht11` | DHT11 温湿度 | GPIO | candidate |
| `dps310` | Infineon DPS310 气压/温度 | I2C | candidate |
| `drv2605` | TI DRV2605 触觉驱动 | I2C | candidate |
| `drv8833` | TI DRV8833 双H桥电机 | GPIO+PWM | candidate |
| `ds18b20` | Maxim DS18B20 温度 | OneWire | candidate |
| `ds3231` | Maxim DS3231 高精度 RTC | I2C | candidate |
| `enc28j60` | Microchip ENC28J60 以太网 | SPI | candidate |
| `ft6x36` | FocalTech FT6X36 触摸 | I2C | candidate |
| `gc9a01` | GC9A01 圆形 TFT 240x240 | SPI | candidate |
| `gpio_button` | GPIO 按键（去抖+长按） | GPIO | candidate |
| `gpio_led` | GPIO LED（闪烁模式） | GPIO | candidate |
| `hcsr04` | HC-SR04 超声波测距 | GPIO | candidate |
| `hd44780` | HD44780 字符 LCD（PCF8574） | I2C | candidate |
| `hmc5883l` | Honeywell HMC5883L 磁力计 | I2C | candidate |
| `ht16k33` | ISSI HT16K33 LED 矩阵 | I2C | candidate |
| `htu21d` | TE HTU21D 温湿度 | I2C | candidate |
| `hx711` | Avia HX711 24-bit 称重 ADC | GPIO | candidate |
| `icm42688` | TDK ICM-42688 六轴 IMU | I2C | candidate |
| `ili9341` | ILI9341 TFT 240x320 | SPI | candidate |
| `ina219` | TI INA219 电流/功率监测 | I2C | candidate |
| `ina226` | TI INA226 精密电流监测 | I2C | candidate |
| `ina3221` | TI INA3221 三通道电流监测 | I2C | candidate |
| `is31fl3731` | ISSI IS31FL3731 LED 矩阵 | I2C | candidate |
| `keypad_matrix` | 矩阵键盘扫描 | GPIO | candidate |
| `lcd_st7789` | ST7789 TFT 240x320 | SPI | candidate |
| `lis2dh12` | ST LIS2DH12 超低功耗加速度计 | I2C | candidate |
| `lis3dh` | ST LIS3DH 三轴加速度计 | I2C | candidate |
| `lis3mdl` | ST LIS3MDL 三轴磁力计 | I2C | candidate |
| `lm75` | NXP LM75 温度 | I2C | candidate |
| `lora_sx1276` | Semtech SX1276 LoRa 收发器 | SPI | candidate |
| `lps22hb` | ST LPS22HB 气压 | I2C | candidate |
| `lsm6dso` | ST LSM6DSO 六轴 IMU | I2C | candidate |
| `ltr390` | Lite-On LTR-390 UV/环境光 | I2C | candidate |
| `max17048` | Maxim MAX17048 电量计 | I2C | candidate |
| `max30102` | Maxim MAX30102 血氧/心率 | I2C | candidate |
| `max31855` | Maxim MAX31855 K型热电偶 | SPI | candidate |
| `max31865` | Maxim MAX31865 RTD PT100 | SPI | candidate |
| `max6675` | Maxim MAX6675 K型热电偶 | SPI | candidate |
| `max7219` | Maxim MAX7219 LED 驱动 | SPI | candidate |
| `mcp23017` | Microchip MCP23017 16-bit IO | I2C | candidate |
| `mcp2515` | Microchip MCP2515 CAN 控制器 | SPI | candidate |
| `mcp3008` | Microchip MCP3008 10-bit ADC 8ch | SPI | candidate |
| `mcp4725` | Microchip MCP4725 12-bit DAC | I2C | candidate |
| `mcp4728` | Microchip MCP4728 4ch 12-bit DAC | I2C | candidate |
| `mlx90614` | Melexis MLX90614 红外温度 | I2C | candidate |
| `mpr121` | NXP MPR121 12ch 电容触摸 | I2C | candidate |
| `mpu6050` | InvenSense MPU6050 六轴 | I2C | candidate |
| `ms5611` | TE MS5611 精密气压 | I2C | candidate |
| `nrf24l01` | Nordic nRF24L01+ 2.4GHz | SPI | candidate |
| `oled_ssd1306` | Solomon SSD1306 OLED 128x64 | I2C | candidate |
| `opt3001` | TI OPT3001 环境光 | I2C | candidate |
| `pac1952` | Microchip PAC1952 双通道功率 | I2C | candidate |
| `pca9685` | NXP PCA9685 16ch PWM 舵机 | I2C | candidate |
| `pcf8563` | NXP PCF8563 RTC | I2C | candidate |
| `pcf8574` | NXP PCF8574 8-bit IO | I2C | candidate |
| `pct2075` | NXP PCT2075 温度 | I2C | candidate |
| `qmc5883p` | QST QMC5883P 磁力计 | I2C | candidate |
| `qmi8658a` | QST QMI8658A 六轴 IMU | I2C | candidate |
| `rc522` | NXP MFRC522 RFID | SPI | candidate |
| `rotary_encoder` | 旋转编码器 | GPIO | candidate |
| `scd40` | Sensirion SCD40 NDIR CO2 | I2C | candidate |
| `sd_spi` | SD 卡 SPI 模式 | SPI | candidate |
| `servo` | 舵机 PWM 控制 | PWM | candidate |
| `sgp30` | Sensirion SGP30 eCO2/TVOC | I2C | candidate |
| `sgp40` | Sensirion SGP40 VOC | I2C | candidate |
| `sh1106` | SH1106 OLED 132x64 | I2C | candidate |
| `sht30` | Sensirion SHT30 温湿度 | I2C | candidate |
| `sht40` | Sensirion SHT40 温湿度 | I2C | candidate |
| `shtc3` | Sensirion SHTC3 低功耗温湿度 | I2C | candidate |
| `si7021` | Silicon Labs Si7021 温湿度 | I2C | candidate |
| `sk6812` | SK6812 RGBW 可寻址 LED | GPIO | candidate |
| `st7735` | ST7735 TFT 128x160 | SPI | candidate |
| `stepper` | 步进电机（梯形加速） | GPIO | candidate |
| `sx1262` | Semtech SX1262 LoRa 收发器 | SPI | candidate |
| `tb6612` | Toshiba TB6612 双H桥电机 | GPIO+PWM | candidate |
| `tca9548a` | TI TCA9548A I2C 多路复用 | I2C | candidate |
| `tcs34725` | ams TCS34725 颜色传感器 | I2C | candidate |
| `tm1637` | TM1637 4位数码管 | GPIO | candidate |
| `tmp102` | TI TMP102 温度 | I2C | candidate |
| `tmp117` | TI TMP117 高精度温度 | I2C | candidate |
| `tsl2591` | ams TSL2591 高动态环境光 | I2C | candidate |
| `veml7700` | Vishay VEML7700 环境光 | I2C | candidate |
| `vl53l0x` | ST VL53L0X ToF 距离 2m | I2C | candidate |
| `vl53l1x` | ST VL53L1X ToF 距离 4m | I2C | candidate |
| `w25qxx` | Winbond W25Q NOR Flash | SPI | candidate |
| `w5500` | WIZnet W5500 TCP/IP 以太网 | SPI | candidate |
| `ws2812` | WS2812 RGB 可寻址 LED | GPIO | candidate |
| `xgzp6816d` | CFSensor XGZP6816D 差压 | I2C | candidate |

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@L-X-Yao
