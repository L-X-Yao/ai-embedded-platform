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

## 目录（49 wheels）

### L0 Primitives（`primitives/` — 21 个）
- `log/` — 带等级 + 模块标签 + 多 sink + 编译期过滤的日志
- `crc16/` — CRC16-MODBUS 校验（table-based）
- `crc32/` — CRC32 IEEE 802.3（table-based）
- `fal/` — Flash 抽象层（vendor: RT-Thread FAL）
- `kvs/` — 键值存储（vendor: FlashDB KVDB）
- `cli_parser/` — 嵌入式调试 shell 的最小 CLI 解析器
- `mahony_ahrs/` — Mahony AHRS 姿态估计滤波器（6/9 轴）
- `mag_cal/` — min/max 磁力计校准（硬铁偏移 + 软铁缩放）
- `pid_controller/` — 离散 PID 控制器（抗积分饱和）
- `moving_average/` — SMA 滑动平均 + EMA 指数平均
- `state_machine/` — 表驱动有限状态机
- `soft_timer/` — 软件定时器（one-shot / repeat）
- `byte_buffer/` — 线性字节缓冲（big-endian put/get）
- `event_queue/` — SPSC 事件队列（无锁 volatile head/tail）
- `bitfield/` — 位级 pack/unpack（MSB-first）
- `retry/` — 重试策略（fixed / linear / exponential backoff）
- `timeout/` — 截止时间追踪（32 位 tick 绕回安全）
- `median_filter/` — 滑动窗口中值滤波（脉冲噪声抑制）
- `checksum/` — XOR / sum8 / Fletcher-16 / LRC / Internet 校验和
- `debounce/` — 软件按键消抖（上升/下降沿检测）
- `led_pattern/` — LED 闪烁模式引擎（单次 / 循环）

### L1 Drivers（`drivers/` — 19 个）
- `w25qxx/` — W25Q 系列 SPI NOR Flash
- `qmi8658a/` — QMI8658A 6 轴 IMU（I2C register-based）
- `qmc5883p/` — QMC5883P 三轴磁力计（I2C register-based）
- `xgzp6816d/` — XGZP6816D 气压计（I2C register-based）
- `gpio_led/` — GPIO LED 控制（闪烁 / 呼吸 / 模式）
- `gpio_button/` — GPIO 按键（消抖 / 长按 / 连击）
- `ws2812/` — WS2812 RGB LED（bit-bang）
- `bmp280/` — Bosch BMP280 温压传感器（I2C register-based）
- `sht30/` — Sensirion SHT30 温湿度（I2C command-based）
- `mpu6050/` — InvenSense MPU-6050 6 轴 IMU（I2C register-based）
- `oled_ssd1306/` — SSD1306 OLED 128x64（I2C cmd+data）
- `dht11/` — DHT11 温湿度（single-wire GPIO）
- `ads1115/` — TI ADS1115 16-bit 4ch ADC（I2C register-based）
- `bh1750/` — ROHM BH1750 光照传感器（I2C command-based）
- `ds18b20/` — Dallas DS18B20 温度（1-Wire）
- `max7219/` — MAX7219 LED 矩阵/七段数码管（SPI）
- `hmc5883l/` — HMC5883L 三轴磁力计（I2C register-based）
- `servo/` — PWM 舵机（角度→脉宽映射）
- `rotary_encoder/` — 正交旋转编码器（软件解码）

### L2 Protocols（`protocols/` — 9 个）
- `at_command/` — AT 指令解析器（URC + 超时 + 多匹配）
- `modbus_rtu/` — Modbus RTU 协议帧编解码
- `ncp5623_parser/` — NCP5623 I2C 命令解析器
- `nmea_parser/` — NMEA 0183 GPS/GNSS 句子解析器
- `ota_ab/` — A/B 固件槽管理 + 回滚
- `mqtt_packet/` — MQTT 3.1.1 无状态序列化/反序列化
- `http_parser/` — HTTP/1.1 零拷贝请求/响应解析
- `websocket_frame/` — WebSocket RFC 6455 帧编解码
- `coap_packet/` — CoAP RFC 7252 消息编解码

### L3 Modules（`modules/`）
（暂无——按项目触发，见 WISHLIST.md）

### Draft（`_draft/`）
（暂无）

全部 49 个 wheel 状态均为 **candidate**，audit 0 non-consumer WARN。

接入方法见 [`docs/integrating-wheels.md`](../docs/integrating-wheels.md)。

## 使用原则

- 新项目**先查 wheels/**，不要从零写
- 发现 wheel 不够用 → **升级 wheel**，不要在项目里搞私有版本
- wheel 修改 → 所有使用它的 `projects/` 需跑回归测试
  - 用 `tooling/scripts/test_downstream.sh <wheel>` 自动发现下游 wheel + 项目并回归测试

## 核心铁律

**新建 wheel 前必须先 `grep vsf/source/` 查 VSF 原生**。VSF 常已提供：

| 你想要的 | VSF 已有 |
|---------|---------|
| 字节 ring buffer | `vsf_fifo_stream` / `vsf_mem_stream` |
| 类型化 FIFO | `vsf_fifo` + `def_vsf_fifo(name, type, depth)` |
| 同步原语 | `vsf_eda_sem / vsf_eda_mutex / vsf_eda_crit / vsf_eda_trig` |
| 列表 | `utilities/vsf_list.h`（slist/dlist） |
| 对象池 | `service/pool/` |
| 堆 | `service/heap/` |
| JSON | `service/json/` |
| 流 | `service/stream/` / `service/simple_stream/` |

详细清单见 [`docs/debugging-playbook.md`](../docs/debugging-playbook.md) 的"VSF 数据结构速查表"。
