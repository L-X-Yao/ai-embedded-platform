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

## 目录清单（52 wheels）

| Wheel | 芯片 | 接口 | 状态 |
|-------|------|------|------|
| `ads1015` | TI ADS1015 12-bit ADC | I2C | candidate |
| `ads1115` | TI ADS1115 16-bit ADC | I2C | candidate |
| `apds9960` | Broadcom APDS-9960 手势/颜色/接近 | I2C | candidate |
| `as5600` | ams AS5600 磁编码器 | I2C | candidate |
| `at24cxx` | Atmel AT24C EEPROM | I2C | candidate |
| `bh1750` | ROHM BH1750 光照传感器 | I2C | candidate |
| `bme280` | Bosch BME280 温湿压 | I2C | candidate |
| `bme680` | Bosch BME680 环境传感器 | I2C | candidate |
| `bmp280` | Bosch BMP280 气压/温度 | I2C | candidate |
| `dht11` | DHT11 温湿度 | 1-Wire | candidate |
| `drv2605` | TI DRV2605 触觉驱动 | I2C | candidate |
| `drv8833` | TI DRV8833 双H桥电机 | GPIO+PWM | candidate |
| `ds18b20` | Maxim DS18B20 温度 | 1-Wire | candidate |
| `ds3231` | Maxim DS3231 高精度 RTC | I2C | candidate |
| `enc28j60` | Microchip ENC28J60 以太网 | SPI | candidate |
| `gpio_button` | GPIO 按键（去抖+长按） | GPIO | candidate |
| `gpio_led` | GPIO LED（闪烁模式） | GPIO | candidate |
| `hmc5883l` | Honeywell HMC5883L 磁力计 | I2C | candidate |
| `hx711` | Avia HX711 24-bit 称重 ADC | GPIO | candidate |
| `ina219` | TI INA219 电流/功率监测 | I2C | candidate |
| `keypad_matrix` | 矩阵键盘扫描 | GPIO | candidate |
| `lis3dh` | ST LIS3DH 三轴加速度计 | I2C/SPI | candidate |
| `lora_sx1276` | Semtech SX1276 LoRa 收发器 | SPI | candidate |
| `max31855` | Maxim MAX31855 热电偶 | SPI | candidate |
| `max31865` | Maxim MAX31865 RTD | SPI | candidate |
| `max7219` | Maxim MAX7219 LED 驱动 | SPI | candidate |
| `mcp23017` | Microchip MCP23017 16-bit IO | I2C | candidate |
| `mcp2515` | Microchip MCP2515 CAN 控制器 | SPI | candidate |
| `mcp4725` | Microchip MCP4725 12-bit DAC | I2C | candidate |
| `mlx90614` | Melexis MLX90614 红外温度 | I2C | candidate |
| `mpu6050` | InvenSense MPU6050 六轴 | I2C | candidate |
| `nrf24l01` | Nordic nRF24L01+ 2.4GHz | SPI | candidate |
| `oled_ssd1306` | Solomon SSD1306 OLED | I2C | candidate |
| `opt3001` | TI OPT3001 环境光 | I2C | candidate |
| `pca9685` | NXP PCA9685 16ch PWM | I2C | candidate |
| `pcf8574` | NXP PCF8574 8-bit IO | I2C | candidate |
| `qmc5883p` | QST QMC5883P 磁力计 | I2C | candidate |
| `qmi8658a` | QST QMI8658A 六轴 IMU | I2C | candidate |
| `rotary_encoder` | 旋转编码器 | GPIO | candidate |
| `sd_spi` | SD 卡 SPI 模式 | SPI | candidate |
| `servo` | 舵机 PWM 控制 | PWM | candidate |
| `sht30` | Sensirion SHT30 温湿度 | I2C | candidate |
| `si7021` | Silicon Labs Si7021 温湿度 | I2C | candidate |
| `stepper` | 步进电机（DIR+STEP） | GPIO+PWM | candidate |
| `tca9548a` | TI TCA9548A I2C 多路复用 | I2C | candidate |
| `tcs34725` | ams TCS34725 颜色传感器 | I2C | candidate |
| `tmp102` | TI TMP102 温度 | I2C | candidate |
| `veml7700` | Vishay VEML7700 环境光 | I2C | candidate |
| `vl53l0x` | ST VL53L0X ToF 距离 | I2C | candidate |
| `w25qxx` | Winbond W25Q NOR Flash | SPI | candidate |
| `ws2812` | WorldSemi WS2812 RGB LED | GPIO | candidate |
| `xgzp6816d` | CFSensor XGZP6816D 气压 | I2C | candidate |
