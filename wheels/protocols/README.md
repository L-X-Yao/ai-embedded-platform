# wheels/protocols/ — L2 协议 / 服务

业界通用协议的封装，常包裹或适配开源实现。

**合格门槛**（L2）：
- sim 里能跑通 loopback（本地模拟通信双方）
- 协议一致性测试（对照标准报文）
- 如果封装了开源库：开源库版本锁定 + 升级策略记录

**"Why not VSF native?" 节必须写**——很多协议 VSF 已有（USB CDC、BLE、btstack 集成等）。

## 典型 wheel 候选

**联网**：
- `mqtt_client` — 选型分析（paho / mongoose / ...） + VSF lwip 集成
- `http_client`
- `http_server`
- `coap_client`

**无线**：
- `ble_uart_service` — BLE UART 透传（VSF 有 btstack 可用）
- `ble_peripheral_framework` — 通用 BLE 从机模式脚手架

**工业**：
- `modbus_rtu_master`
- `modbus_tcp_master`

**串口**：
- `at_command` — AT 指令解析器（WiFi/BLE 模组常用）

**USB**：
- VSF 已原生支持主从机 + CDC/HID/MSC/UVC，通常无需封装

## 目录清单

（待添加）
