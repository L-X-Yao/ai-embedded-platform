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

## 目录

### L0 Primitives（`primitives/`）
- `log/` — 带等级 + 模块标签 + 多 sink + 编译期过滤的日志（candidate）
- `crc16/` — CRC16-MODBUS 校验（table-based）（candidate）
- `cli_parser/` — 嵌入式调试 shell 的最小 CLI 解析器（candidate）
- `fal/` — Flash 抽象层（vendor: RT-Thread FAL）（candidate）
- `kvs/` — 键值存储（vendor: FlashDB KVDB）（candidate）
- `mahony_ahrs/` — Mahony AHRS 姿态估计滤波器（6/9 轴）（candidate）
- `mag_cal/` — min/max 磁力计校准（硬铁偏移 + 软铁缩放）（candidate）

### L1 Drivers（`drivers/`）
- `w25qxx/` — W25Q 系列 SPI NOR Flash 驱动（candidate）
- `qmi8658a/` — QMI8658A 6 轴 IMU 驱动（I2C）（candidate）
- `qmc5883p/` — QMC5883P 三轴磁力计驱动（I2C）（candidate）
- `xgzp6816d/` — XGZP6816D 气压计驱动（I2C）（candidate）
- `gpio_led/` — GPIO LED 控制（闪烁 / 呼吸 / 模式）（candidate）
- `gpio_button/` — GPIO 按键（消抖 / 长按 / 连击）（candidate）
- `ws2812/` — WS2812 RGB LED 驱动（bit-bang）（candidate）

### L2 Protocols（`protocols/`）
- `at_command/` — AT 指令解析器（URC + 超时 + 多匹配）（candidate）
- `modbus_rtu/` — Modbus RTU 协议帧编解码（candidate）
- `ncp5623_parser/` — NCP5623 I2C 命令解析器（candidate）
- `nmea_parser/` — NMEA 0183 GPS/GNSS 句子解析器（candidate）

### L3 Modules（`modules/`）
（暂无——按项目触发，见 WISHLIST.md）

### Draft（`_draft/`）
（暂无）

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
