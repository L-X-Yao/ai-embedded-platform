# Wheels Wishlist

> ⚠️ **这不是 TODO，不是路线图**。
>
> 只是"将来某个项目可能需要的候选"的记忆库——等真实项目触发时从这里挑作为起点。
>
> **不要**按这个清单挨个做：
> - 没真实需求触发的 wheel 90% 抽象错方向
> - 清单里列的技术 60% 可能永远不做（项目方向变）
> - 清单长 ≠ 工作量大。**只是头脑风暴**
>
> 添加条目的门槛**极低**（想到就加）。做条目的门槛**极高**（必须有项目触发 + API review 通过）。

每个条目要写：
- 大致功能
- 预计层级（L0/L1/L2/L3）
- VSF 是否已有相近实现（需核实）

---

## L0 · 工具原语

| 候选 | 功能 | VSF 原生检查 |
|------|------|-------------|
| `log` | 带等级（debug/info/warn/error）+ 模块标签的日志，路由到多个 sink | VSF 有 `vsf_trace`（trace/warning/error），结构略差 |
| `cli_parser` | getopt-like 命令行 + 命令分发 | VSF linux skin 有 `getopt`，无高级命令分发 |
| `state_machine` | 层次/嵌套状态机模板（加 OO 封装） | VSF 有 `vsf_fsm`（简单）和 EDA 状态编程模型 |
| `retry` | 指数退避重试封装 | VSF 无 |
| `crc` | CRC8/16/32 常用多项式 | VSF 有 `service/hash/crc/`（需核实覆盖度） |
| `base64` | base64 编解码 | VSF 无 |

## L1 · 硬件驱动（按芯片型号，不是泛称）

**传感器**：
- `mpu6050` / `mpu9250` — 六轴/九轴 IMU
- `bme280` / `bmp280` — 温湿度气压
- `bh1750` — 光照
- `ds18b20` — 单总线温度
- `sht3x` — 温湿度

**存储**：
- `w25qxx` — SPI NOR flash 家族
- `at24cxx` — I2C EEPROM 家族
- `sd_spi` — SD 卡 SPI 模式

**显示**：
- `ssd1306` — OLED（I2C/SPI）
- `st7789` — TFT LCD
- `ili9341` — TFT LCD

**通信模组**：
- `esp_at` — ESP WiFi 模组 AT 交互
- `ec800` / `n58` — 4G 模组 AT

**其他**：
- `pcf8574` — I2C IO 扩展
- `mcp23017` — I2C IO 扩展（16 位）
- `tm1637` / `tm1638` — 数码管驱动
- `gpio_led` — LED 状态机（闪烁/呼吸/脉冲）
- `gpio_button` — 按键消抖 + 长按/双击

## L2 · 协议 / 服务

**联网**：
- `mqtt_client` — MQTT 3.1.1/5.0（选型：paho / mongoose / mqttnet）
- `http_client` — HTTP/1.1 客户端
- `http_server` — 简单 HTTP 服务器
- `websocket_client` — WebSocket
- `coap_client` — CoAP（IoT 低功耗）
- `ntp_client` — 时间同步（VSF 可能有）

**无线**：
- `ble_uart_service` — BLE UART 透传 Service/Characteristic
- `ble_peripheral_framework` — BLE 从机脚手架（含广播/连接/断线重连）
- `wifi_manager` — WiFi 扫描/连接/重连

**工业**：
- `modbus_rtu_master` — Modbus RTU 主站
- `modbus_rtu_slave` — 从站
- `modbus_tcp_master` / `_slave`

**串口交互**：
- `at_command` — AT 指令解析器（WiFi/BLE 模组用）
- `shell_uart` — 通过 UART 对外提供命令 shell

**数据**：
- `json_rpc` — JSON-RPC 2.0 客户端/服务端
- `protobuf_nanopb` — protobuf 小型化（选型：nanopb）

## L3 · 功能模块

**升级**：
- `ota_updater` — OTA 固件升级（含版本检查、断点续传、回滚、签名验证）
- `bootloader_base` — 通用 bootloader 骨架（双 bank / A-B slot）

**数据**：
- `data_logger` — 采集 → 缓冲 → 存储 → 上传（依赖 sensor wheel + storage wheel）
- `event_recorder` — 环形事件日志到 flash（含黑匣子场景）

**管理**：
- `config_manager` — 配置持久化（依赖 kvs）
- `cli_shell` — 完整的嵌入式 shell（包装 cli_parser + 命令注册 + 历史）
- `sys_monitor` — 任务/内存/CPU 占用监控 + 告警

**网络应用**：
- `network_manager` — WiFi/4G/以太网多路切换
- `device_shadow` — 设备影子（类 AWS IoT）
- `mqtt_device_client` — 集成式设备端（MQTT + 影子 + OTA）

## GUI

- `lvgl_integration` — LVGL 与 VSF 的胶水层（事件 / 定时 / 显示驱动）
- `tgui_integration` — VSF 原生 tgui 使用封装（VSF 已有 tgui 组件）

## 存储

- `kvs` — Key-Value 存储（对 flash 的抽象）
- `ring_log_flash` — 环形日志写 flash（用 vsf_fifo_stream 包装）
- `rrd_storage` — 轮询式时序数据（类似 rrdtool 简化版）

---

## 添加新候选的格式

```markdown
- `<wheel-name>` — <一句话功能> | VSF 原生：<有/无/部分覆盖> | 触发项目：<哪个项目会用>
```

遇到新需求先来这里查有没有想过。**实现**时才按 `ai-workflow.md` 任务类型 1 走完整流程。
