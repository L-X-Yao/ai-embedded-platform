# wheels/primitives/ — L0 工具原语

纯软件工具，无硬件依赖，所有项目通用。

**合格门槛最严**（L0）：
- sim 100% 能跑（VSF linux host）
- 测试覆盖 ≥ 80%
- API 简洁（通常 <10 个函数）
- 必须有 "Why not VSF native?" 节

## 典型 wheel 候选

- ~~`log`~~ — **已实现**，见 `log/README.md`
- `cli_parser` — 命令/参数解析
- `state_machine` — 层次状态机模板（考虑：VSF 已有 `vsf_eda` 的 FSM 模式）
- `retry` — 指数退避重试
- `crc` — CRC8/16/32 常用多项式

**不做**：
- ring_buffer（VSF 有 `vsf_fifo_stream`）
- FIFO（VSF 有 `vsf_fifo`）
- 任何 VSF 已原生提供的

## 目录清单

- `log/` — 带等级 + 模块标签 + 多 sink + 编译期过滤的日志（candidate）
