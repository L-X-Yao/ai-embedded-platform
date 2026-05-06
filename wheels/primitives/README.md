# wheels/primitives/ — L0 工具原语

纯软件工具，无硬件依赖，所有项目通用。

**合格门槛最严**（L0）：
- sim 100% 能跑（VSF linux host）
- 测试覆盖 ≥ 80%
- API 简洁（通常 <10 个函数）
- 必须有 "Why not VSF native?" 节

**不做**：
- FIFO（VSF 有 `vsf_fifo`）
- 任何 VSF 已原生提供的

## 目录清单（51 wheels）

| Wheel | 功能 | 状态 |
|-------|------|------|
| `aes128` | AES-128 对称加密 | candidate |
| `base64` | Base64 编解码 | candidate |
| `bitfield` | 位域读写工具 | candidate |
| `bitmap` | 位图操作 | candidate |
| `byte_buffer` | 线性字节缓冲区 | candidate |
| `checksum` | 校验和算法集 | candidate |
| `cli_parser` | 命令行解析器 | candidate |
| `cobs` | COBS 字节填充编码 | candidate |
| `crc16` | CRC16 多项式 | candidate |
| `crc32` | CRC32 IEEE 802.3 | candidate |
| `crc8` | CRC8 多项式 | candidate |
| `debounce` | 信号去抖 | candidate |
| `endian` | 字节序转换 | candidate |
| `event_queue` | 事件队列（SPSC） | candidate |
| `fal` | Flash 抽象层（vendor） | candidate |
| `fifo_queue` | 类型化 FIFO 队列 | candidate |
| `fixed_point` | 定点数运算 | candidate |
| `hash_fnv` | FNV-1a 哈希 | candidate |
| `hex_codec` | 十六进制编解码 | candidate |
| `json_parser` | JSON 解析器（零分配） | candidate |
| `kvs` | 键值存储（vendor FlashDB） | candidate |
| `led_pattern` | LED 闪烁模式引擎 | candidate |
| `log` | 带等级+模块+多 sink 的日志 | candidate |
| `log_color` | ANSI 彩色日志 sink | candidate |
| `log_dedup` | 日志去重 sink | candidate |
| `log_file` | FILE* 日志 sink | candidate |
| `log_filter` | 模块过滤日志 sink | candidate |
| `log_flash` | Flash 持久化日志 sink | candidate |
| `log_rate` | 令牌桶限流日志 sink | candidate |
| `log_ringbuf` | RAM 环形日志 sink | candidate |
| `log_rtt` | RTT 日志 sink | candidate |
| `log_uart` | UART 日志 sink | candidate |
| `lookup_table` | 查表工具（线性插值） | candidate |
| `mag_cal` | 磁力计校准（min/max） | candidate |
| `mahony_ahrs` | Mahony 互补滤波器 | candidate |
| `median_filter` | 中值滤波器 | candidate |
| `moving_average` | 滑动平均 | candidate |
| `moving_window` | 滑动窗口统计 | candidate |
| `pid_controller` | PID 控制器 | candidate |
| `printf_tiny` | 精简 printf（无浮点/无堆） | candidate |
| `rate_limiter` | 速率限制器（令牌桶） | candidate |
| `retry` | 指数退避重试 | candidate |
| `ring_buffer` | 字节环形缓冲区 | candidate |
| `sha256` | SHA-256 哈希 | candidate |
| `slip` | SLIP 帧编解码 | candidate |
| `soft_timer` | 软件定时器管理 | candidate |
| `state_machine` | 层次状态机 | candidate |
| `string_utils` | 字符串工具函数 | candidate |
| `timeout` | 超时检测 | candidate |
| `timer_wheel` | 时间轮定时器 | candidate |
| `uuid` | UUID 生成/格式化 | candidate |
