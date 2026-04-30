# wheels/primitives/ — L0 工具原语

纯软件工具，无硬件依赖，所有项目通用。

**合格门槛最严**（L0）：
- sim 100% 能跑（VSF linux host）
- 测试覆盖 ≥ 80%
- API 简洁（通常 <10 个函数）
- 必须有 "Why not VSF native?" 节

**不做**：
- ring_buffer（VSF 有 `vsf_fifo_stream`）
- FIFO（VSF 有 `vsf_fifo`）
- 任何 VSF 已原生提供的

## 目录清单（30 wheels）

| Wheel | 功能 | 状态 |
|-------|------|------|
| `bitfield` | 位域读写工具 | candidate |
| `byte_buffer` | 线性字节缓冲区 | candidate |
| `checksum` | 校验和算法集 | candidate |
| `cli_parser` | 命令行解析器 | candidate |
| `crc16` | CRC16 多项式 | candidate |
| `crc32` | CRC32 IEEE 802.3 | candidate |
| `debounce` | 信号去抖 | candidate |
| `event_queue` | 事件队列（SPSC） | candidate |
| `fal` | Flash 抽象层（vendor） | candidate |
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
| `mag_cal` | 磁力计校准（min/max） | candidate |
| `mahony_ahrs` | Mahony 互补滤波器 | candidate |
| `median_filter` | 中值滤波器 | candidate |
| `moving_average` | 滑动平均 | candidate |
| `pid_controller` | PID 控制器 | candidate |
| `retry` | 指数退避重试 | candidate |
| `soft_timer` | 软件定时器管理 | candidate |
| `state_machine` | 层次状态机 | candidate |
| `timeout` | 超时检测 | candidate |
