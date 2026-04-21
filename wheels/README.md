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
（待添加）

### L1 Drivers（`drivers/`）
（待添加——按芯片型号命名：`mpu6050/`、`w25q64/` 等）

### L2 Protocols（`protocols/`）
（待添加）

### L3 Modules（`modules/`）
（待添加）

### Draft（`_draft/`）
（待添加——孵化中）

## 使用原则

- 新项目**先查 wheels/**，不要从零写
- 发现 wheel 不够用 → **升级 wheel**，不要在项目里搞私有版本
- wheel 修改 → 跑 `tooling/scripts/test_downstream.sh <wheel>` 验证所有下游项目

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
