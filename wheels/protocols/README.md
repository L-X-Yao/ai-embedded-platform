# wheels/protocols/ — L2 协议 / 服务

业界通用协议的封装。

**合格门槛**（L2）：
- sim 里能跑通 loopback（本地模拟通信双方）
- 协议一致性测试（对照标准报文）
- 如果封装了开源库：开源库版本锁定 + 升级策略记录

**"Why not VSF native?" 节必须写**——很多协议 VSF 已有（USB CDC、BLE、btstack 集成等）。

## 目录清单（25 wheels）

| Wheel | 协议 | 状态 |
|-------|------|------|
| `at_command` | AT 命令解析器 | candidate |
| `bencode` | Bencode 编解码（BitTorrent 格式） | candidate |
| `canbus` | CAN 总线帧编解码 | candidate |
| `cbor_lite` | CBOR 轻量编解码（RFC 7049） | candidate |
| `coap_packet` | CoAP 报文编解码（RFC 7252） | candidate |
| `crc_any` | 通用 CRC 计算（可配多项式/位宽） | candidate |
| `dns_packet` | DNS 报文编解码 | candidate |
| `http_parser` | HTTP/1.1 请求/响应解析 | candidate |
| `i2c_scan` | I2C 总线扫描工具 | candidate |
| `lorawan_packet` | LoRaWAN 报文编解码 | candidate |
| `midi_packet` | MIDI 消息编解码 | candidate |
| `modbus_rtu` | Modbus RTU master/slave | candidate |
| `modbus_tcp` | Modbus TCP 帧编解码 | candidate |
| `mqtt_packet` | MQTT 3.1.1 报文编解码 | candidate |
| `mqtt5_packet` | MQTT 5.0 报文编解码 | candidate |
| `msgpack` | MessagePack 编解码 | candidate |
| `ncp5623_parser` | NCP5623 I2C LED 协议解析 | candidate |
| `nmea_parser` | NMEA 0183 GPS 语句解析 | candidate |
| `ota_ab` | OTA A/B 槽管理 | candidate |
| `protobuf_lite` | Protocol Buffers 轻量编解码 | candidate |
| `syslog_packet` | Syslog RFC 5424 报文编解码 | candidate |
| `tlv_codec` | TLV（Tag-Length-Value）编解码 | candidate |
| `websocket_frame` | WebSocket 帧编解码（RFC 6455） | candidate |
| `xmodem` | XMODEM 文件传输协议 | candidate |
| `ymodem` | YMODEM 文件传输协议 | candidate |
