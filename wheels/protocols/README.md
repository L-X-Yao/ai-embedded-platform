# wheels/protocols/ — L2 协议 / 服务

业界通用协议的封装。

**合格门槛**（L2）：
- sim 里能跑通 loopback（本地模拟通信双方）
- 协议一致性测试（对照标准报文）
- 如果封装了开源库：开源库版本锁定 + 升级策略记录

**"Why not VSF native?" 节必须写**——很多协议 VSF 已有（USB CDC、BLE、btstack 集成等）。

## 目录清单（9 wheels）

| Wheel | 协议 | 状态 |
|-------|------|------|
| `at_command` | AT 命令解析器 | candidate |
| `coap_packet` | CoAP 报文编解码（RFC 7252） | candidate |
| `http_parser` | HTTP/1.1 请求/响应解析 | candidate |
| `modbus_rtu` | Modbus RTU master/slave | candidate |
| `mqtt_packet` | MQTT 3.1.1 报文编解码 | candidate |
| `ncp5623_parser` | NCP5623 I2C LED 协议解析 | candidate |
| `nmea_parser` | NMEA 0183 GPS 语句解析 | candidate |
| `ota_ab` | OTA A/B 槽管理 | candidate |
| `websocket_frame` | WebSocket 帧编解码（RFC 6455） | candidate |
