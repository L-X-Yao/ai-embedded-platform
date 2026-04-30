# wheels/drivers/ — L1 硬件驱动

具体芯片/传感器/外设的驱动，通过 HAL 函数指针注入跨 MCU。

**命名**：按芯片/器件型号，不按泛称。
- ✅ `mpu6050/`（六轴传感器）
- ✅ `w25qxx/`（SPI flash 家族）
- ❌ `display/`（泛名，不同芯片差异大）

**合格门槛**（L1）：
- sim 里有 mock 测试（模拟硬件行为）
- 至少 1 次真机验证记录（写入 README 的"支持的平台"节，含日期）
- 所有硬件访问必须走 HAL 函数指针注入，**禁止**直接操作寄存器

## 目录清单（19 wheels）

| Wheel | 芯片 | 接口 | 状态 |
|-------|------|------|------|
| `ads1115` | TI ADS1115 16-bit ADC | I2C | candidate |
| `bh1750` | ROHM BH1750 光照传感器 | I2C | candidate |
| `bmp280` | Bosch BMP280 气压/温度 | I2C | candidate |
| `dht11` | DHT11 温湿度 | 1-Wire | candidate |
| `ds18b20` | Maxim DS18B20 温度 | 1-Wire | candidate |
| `gpio_button` | GPIO 按键（去抖+长按） | GPIO | candidate |
| `gpio_led` | GPIO LED（闪烁模式） | GPIO | candidate |
| `hmc5883l` | Honeywell HMC5883L 磁力计 | I2C | candidate |
| `max7219` | Maxim MAX7219 LED 驱动 | SPI | candidate |
| `mpu6050` | InvenSense MPU6050 六轴 | I2C | candidate |
| `oled_ssd1306` | Solomon SSD1306 OLED | I2C | candidate |
| `qmc5883p` | QST QMC5883P 磁力计 | I2C | candidate |
| `qmi8658a` | QST QMI8658A 六轴 IMU | I2C | candidate |
| `rotary_encoder` | 旋转编码器 | GPIO | candidate |
| `servo` | 舵机 PWM 控制 | PWM | candidate |
| `sht30` | Sensirion SHT30 温湿度 | I2C | candidate |
| `w25qxx` | Winbond W25Q NOR Flash | SPI | candidate |
| `ws2812` | WorldSemi WS2812 RGB LED | GPIO | candidate |
| `xgzp6816d` | CFSensor XGZP6816D 气压 | I2C | candidate |
