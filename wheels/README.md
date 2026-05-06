# wheels/

经过验证的可复用模块——平台核心资产。

## 分层（Level）

| 层 | 目录 | 内容 | 大小 | 合格门槛 |
|----|------|------|------|---------|
| **L0 原语** | `primitives/` | 纯软件工具：日志、状态机、CRC 等 | 50-500 行 | sim 通过 + ≥80% 测试覆盖 |
| **L1 驱动** | `drivers/` | 具体芯片/外设：MPU6050、W25Q64、LCD 等 | 100-1500 行 | sim mock + ≥1 真机验收 |
| **L2 协议** | `protocols/` | MQTT、BLE service、AT 指令等 | 500-5000 行 | sim loopback + 一致性测试 |
| **L3 模块** | `modules/` | OTA、data logger、shell 等高层功能 | 1000-8000 行 | 集成测试 + 真机验收 |

**孵化**：新 wheel 先放 `_draft/<name>/`，达标后升到对应层。

## 状态级别

- `draft`（`_draft/` 或各层下 + `STATUS: draft`）：代码能跑 + 有 sim 测试，但未充分验证
- `candidate`（`STATUS: candidate`）：wheel-contract ≥ 70% 满足
- `stable`（`STATUS: stable`）：全部 contract 满足 + 至少 2 个真项目用过

见 [`docs/wheel-contract.md`](../docs/wheel-contract.md) 了解升级标准。

## 目录（167 wheels）

### L0 Primitives（`primitives/` — 56 个）
- `aes128/` — AES-128 对称加密
- `base64/` — Base64 编解码
- `bitfield/` — 位域读写工具（MSB-first pack/unpack）
- `bitmap/` — 位图操作（set/clear/test/find）
- `byte_buffer/` — 线性字节缓冲区（big-endian put/get）
- `checksum/` — 校验和算法集（XOR/sum8/Fletcher-16/LRC/Internet）
- `cli_parser/` — 嵌入式调试 shell 的最小 CLI 解析器
- `cobs/` — COBS 一致性字节填充编码
- `crc16/` — CRC16-MODBUS 校验（table-based）
- `crc32/` — CRC32 IEEE 802.3（table-based）
- `crc8/` — CRC8 校验（table-based）
- `debounce/` — 软件按键消抖（上升/下降沿检测）
- `endian/` — 字节序转换工具
- `event_queue/` — SPSC 事件队列（无锁 volatile head/tail）
- `fal/` — Flash 抽象层（自写，API 兼容 FAL）
- `fifo_queue/` — 类型化 FIFO 队列
- `fixed_point/` — 定点数运算（Q16.16）
- `hash_fnv/` — FNV-1a 哈希
- `hex_codec/` — 十六进制编解码
- `iir_lowpass/` — 一阶 IIR 低通滤波器（EMA）
- `json_parser/` — JSON 解析器（零分配）
- `kalman_1d/` — 1D 标量卡尔曼滤波器
- `kvs/` — 键值存储（vendor: FlashDB KVDB）
- `led_pattern/` — LED 闪烁模式引擎（单次/循环）
- `log/` — 带等级 + 模块标签 + 多 sink + 编译期过滤的日志
- `log_color/` — ANSI 彩色日志 sink
- `log_dedup/` — 日志去重 sink
- `log_file/` — FILE* 日志 sink
- `log_filter/` — 模块过滤日志 sink
- `log_flash/` — Flash 持久化日志 sink
- `log_rate/` — 令牌桶限流日志 sink
- `log_ringbuf/` — RAM 环形日志 sink
- `log_rtt/` — RTT 日志 sink
- `log_uart/` — UART 日志 sink
- `lookup_table/` — 查表工具（线性插值）
- `mag_cal/` — 磁力计校准（min/max 硬铁偏移 + 软铁缩放）
- `mahony_ahrs/` — Mahony AHRS 姿态估计滤波器（6/9 轴）
- `median_filter/` — 滑动窗口中值滤波（脉冲噪声抑制）
- `moving_average/` — SMA 滑动平均 + EMA 指数平均
- `moving_window/` — 滑动窗口统计（min/max/mean）
- `pid_controller/` — 离散 PID 控制器（抗积分饱和）
- `printf_tiny/` — 精简 printf 实现（无浮点/无堆）
- `rate_limiter/` — 速率限制器（令牌桶）
- `retry/` — 重试策略（fixed/linear/exponential backoff）
- `ring_buffer/` — 字节环形缓冲区
- `sha256/` — SHA-256 哈希
- `slip/` — SLIP 串行线路 IP 帧编解码
- `soft_timer/` — 软件定时器管理（one-shot/repeat）
- `state_machine/` — 表驱动有限状态机
- `string_utils/` — 字符串工具函数
- `timeout/` — 截止时间追踪（32 位 tick 绕回安全）
- `timer_wheel/` — 时间轮定时器
- `uuid/` — UUID 生成/格式化

### L1 Drivers（`drivers/` — 52 个）
- `ads1015/` — TI ADS1015 12-bit 4ch ADC（I2C）
- `ads1115/` — TI ADS1115 16-bit 4ch ADC（I2C）
- `apds9960/` — Broadcom APDS-9960 手势/颜色/接近（I2C）
- `as5600/` — ams AS5600 磁编码器（I2C）
- `at24cxx/` — Atmel AT24C 系列 EEPROM（I2C）
- `bh1750/` — ROHM BH1750 光照传感器（I2C）
- `bme280/` — Bosch BME280 温湿压传感器（I2C）
- `bme680/` — Bosch BME680 环境传感器（I2C）
- `bmp280/` — Bosch BMP280 气压/温度（I2C）
- `dht11/` — DHT11 温湿度（single-wire GPIO）
- `drv2605/` — TI DRV2605 触觉驱动（I2C）
- `drv8833/` — TI DRV8833 双H桥电机驱动（GPIO+PWM）
- `ds18b20/` — Maxim DS18B20 温度（1-Wire）
- `ds3231/` — Maxim DS3231 高精度 RTC（I2C）
- `enc28j60/` — Microchip ENC28J60 以太网控制器（SPI）
- `gpio_button/` — GPIO 按键（消抖/长按/连击）
- `gpio_led/` — GPIO LED 控制（闪烁/呼吸/模式）
- `hmc5883l/` — Honeywell HMC5883L 三轴磁力计（I2C）
- `hx711/` — Avia HX711 24-bit 称重 ADC（GPIO）
- `ina219/` — TI INA219 电流/功率监测（I2C）
- `keypad_matrix/` — 矩阵键盘扫描（GPIO）
- `lis3dh/` — ST LIS3DH 三轴加速度计（I2C/SPI）
- `lora_sx1276/` — Semtech SX1276 LoRa 收发器（SPI）
- `max31855/` — Maxim MAX31855 热电偶（SPI）
- `max31865/` — Maxim MAX31865 RTD（SPI）
- `max7219/` — Maxim MAX7219 LED 矩阵/七段驱动（SPI）
- `mcp23017/` — Microchip MCP23017 16-bit IO 扩展（I2C）
- `mcp2515/` — Microchip MCP2515 CAN 控制器（SPI）
- `mcp4725/` — Microchip MCP4725 12-bit DAC（I2C）
- `mlx90614/` — Melexis MLX90614 红外温度（I2C）
- `mpu6050/` — InvenSense MPU-6050 6 轴 IMU（I2C）
- `nrf24l01/` — Nordic nRF24L01+ 2.4GHz 收发器（SPI）
- `oled_ssd1306/` — Solomon SSD1306 OLED 128x64（I2C）
- `opt3001/` — TI OPT3001 环境光传感器（I2C）
- `pca9685/` — NXP PCA9685 16ch PWM 驱动（I2C）
- `pcf8574/` — NXP PCF8574 8-bit IO 扩展（I2C）
- `qmc5883p/` — QST QMC5883P 三轴磁力计（I2C）
- `qmi8658a/` — QST QMI8658A 6 轴 IMU（I2C）
- `rotary_encoder/` — 正交旋转编码器（GPIO 软件解码）
- `sd_spi/` — SD 卡 SPI 模式驱动
- `servo/` — PWM 舵机（角度到脉宽映射）
- `sht30/` — Sensirion SHT30 温湿度（I2C）
- `si7021/` — Silicon Labs Si7021 温湿度（I2C）
- `stepper/` — 步进电机驱动（DIR+STEP）
- `tca9548a/` — TI TCA9548A I2C 多路复用器（I2C）
- `tcs34725/` — ams TCS34725 颜色传感器（I2C）
- `tmp102/` — TI TMP102 温度传感器（I2C）
- `veml7700/` — Vishay VEML7700 环境光传感器（I2C）
- `vl53l0x/` — ST VL53L0X ToF 距离传感器（I2C）
- `w25qxx/` — Winbond W25Q 系列 SPI NOR Flash
- `ws2812/` — WorldSemi WS2812 RGB LED（bit-bang GPIO）
- `xgzp6816d/` — CFSensor XGZP6816D 气压计（I2C）

### L2 Protocols（`protocols/` — 25 个）
- `at_command/` — AT 指令解析器（URC + 超时 + 多匹配）
- `bencode/` — Bencode 编解码（BitTorrent 格式）
- `canbus/` — CAN 总线帧编解码
- `cbor_lite/` — CBOR 轻量编解码（RFC 7049）
- `coap_packet/` — CoAP RFC 7252 消息编解码
- `crc_any/` — 通用 CRC 计算（可配多项式/位宽）
- `dns_packet/` — DNS 报文编解码
- `http_parser/` — HTTP/1.1 零拷贝请求/响应解析
- `i2c_scan/` — I2C 总线扫描工具
- `lorawan_packet/` — LoRaWAN 报文编解码
- `midi_packet/` — MIDI 消息编解码
- `modbus_rtu/` — Modbus RTU master/slave 帧编解码
- `modbus_tcp/` — Modbus TCP 帧编解码
- `mqtt_packet/` — MQTT 3.1.1 无状态序列化/反序列化
- `mqtt5_packet/` — MQTT 5.0 报文编解码
- `msgpack/` — MessagePack 编解码
- `ncp5623_parser/` — NCP5623 I2C LED 协议解析
- `nmea_parser/` — NMEA 0183 GPS/GNSS 句子解析
- `ota_ab/` — A/B 固件槽管理 + 回滚
- `protobuf_lite/` — Protocol Buffers 轻量编解码
- `syslog_packet/` — Syslog RFC 5424 报文编解码
- `tlv_codec/` — TLV（Tag-Length-Value）编解码
- `websocket_frame/` — WebSocket RFC 6455 帧编解码
- `xmodem/` — XMODEM 文件传输协议
- `ymodem/` — YMODEM 文件传输协议

### L3 Modules（`modules/` — 18 个）
- `boot_counter/` — 启动计数器（Flash 持久化）
- `button_handler/` — 按键事件处理（多按键/组合键）
- `cli_shell/` — 完整命令行 shell（命令注册 + 历史）
- `command_queue/` — 命令队列（异步命令调度）
- `config_store/` — 配置持久化存储
- `data_logger/` — 数据采集 -> 存储 -> 上传
- `device_info/` — 设备信息查询（型号/版本/序列号）
- `error_handler/` — 错误处理框架（错误码 + 回调）
- `event_bus/` — 事件总线（发布/订阅）
- `flash_journal/` — Flash 日志（追加写 + 磨损均衡）
- `ota_manager/` — OTA 固件更新管理
- `param_table/` — 参数表（运行时可调参数）
- `ring_log/` — 环形日志（固定大小 RAM 缓冲）
- `scheduler/` — 任务调度器（协作式）
- `sensor_hub/` — 传感器集线器（多传感器统一采集）
- `task_watchdog/` — 任务看门狗（多任务喂狗监控）
- `telemetry/` — 遥测数据打包上报
- `version_info/` — 版本信息管理（编译时注入）

### Draft（`_draft/`）
（暂无）

全部 146 个 wheel 状态均为 **candidate**，audit 0 open findings。

接入方法见 [`docs/integrating-wheels.md`](../docs/integrating-wheels.md)。

## 使用原则

- 新项目**先查 wheels/**，不要从零写
- 发现 wheel 不够用 → **升级 wheel**，不要在项目里搞私有版本
- wheel 修改 → 所有使用它的 `projects/` 需跑回归测试
  - 用 `tooling/test/test_downstream.sh <wheel>` 自动发现下游 wheel + 项目并回归测试

## 核心铁律

**新建 wheel 前建议先 `grep vsf/source/` 查 VSF 原生**。VSF 常已提供：

| 你想要的 | VSF 已有 |
|---------|---------|
| 类型化 FIFO | `vsf_fifo` + `def_vsf_fifo(name, type, depth)` |
| 同步原语 | `vsf_eda_sem / vsf_eda_mutex / vsf_eda_crit / vsf_eda_trig` |
| 列表 | `utilities/vsf_list.h`（slist/dlist） |
| 对象池 | `service/pool/` |
| 堆 | `service/heap/` |
| JSON | `service/json/` |
| 流 | `service/stream/` / `service/simple_stream/` |

详细清单见 [`docs/debugging-playbook.md`](../docs/debugging-playbook.md) 的"VSF 数据结构速查表"。
