# ADR 006: 新 wheel 必须先 grep VSF 原生，不重造已有能力

**状态**：accepted
**日期**：2026-04-21
**决策人**：@user

## 背景

2026-04-21 创建的第一个 wheel `ring_buffer` 被用户指出：VSF 已有 `vsf_fifo_stream` 等价物（字节流 + 事件机制）。该 wheel 被删除。

AI 有"我写简单 C 比读 PLOOC 宏容易"的倾向，如果不强制约束，会重造 VSF 已有能力。长期积累会让平台 wheels 里 20-30% 是 VSF 套壳，浪费资源 + 使用者精神分裂（不知道该用哪个）。

## 考虑过的选项

### A. 靠 AI 自觉（已证明不行）
- AI 写完 ai-workflow 规则后立刻违反（ring_buffer 事件）

### B. 代码审查拦截
- 优点：人把关
- 缺点：违背"AI 90%"目标，人成为瓶颈

### C. 机制强制：合同要求 "Why not VSF native?" 节（选中）✓
- 每个新 wheel 的 README **必须**有此节
- 列出检查过的 VSF 等价物 + 为何不够用
- 缺此节的 wheel 不能升级到 draft 之上

## 决策

**选 C：强制"Why not VSF native?" README 节**。

写入：
- `docs/wheel-contract.md` §1（零号铁律）
- `docs/ai-workflow.md` 任务类型 1 第 2 步
- `docs/debugging-playbook.md` VSF 数据结构速查表

### 速查表（记不住时查 debugging-playbook.md）

- 字节 ring buffer → `vsf_fifo_stream` / `vsf_mem_stream`
- 类型化 FIFO → `vsf_fifo` + `def_vsf_fifo(name, type, depth)`
- 同步原语 → `vsf_eda_sem / mutex / crit / trig / queue`
- 列表 → `vsf_slist_t / vsf_dlist_t`（`utilities/vsf_list.h`）
- 对象池 → `service/pool/`
- 堆 → `service/heap/`
- JSON → `service/json/`
- 哈希/CRC → `service/hash/`
- 流 → `service/stream/` / `service/simple_stream/`

### 强制检查顺序

AI 新建 wheel 流程：
0. **（仅板级能力判定需要，纯算法/数据结构跳过）** `cat vsf/example/template/config/vsf_usr_cfg/vsf_usr_cfg_<board>.h` — VSF 官方对每块板能力的"真相表"。看哪些 `VSF_USE_*` 被注释（`//#define`）、哪些 `APP_USE_*_DEMO` 默认 DISABLED，即知 VSF 作者判定该 board 不适合此路。**比读 HAL 源码快 10 倍**。来源：2026-04-23 USB probe（`vsf_usr_cfg_mps2.h:91-92` 明文 "USB device will be enabled if target chip supports USBD"，直接证伪 "USB device sim on mps2" 方向）
1. `grep -rn "功能关键词" vsf/source/service/`
2. `grep -rn "功能关键词" vsf/source/utilities/`
3. `grep -rn "功能关键词" vsf/source/component/`
4. 写 "Why not VSF native?" 草案
5. 有足够理由 → 继续
6. 没足够理由 → **放弃**，文档化"用 vsf_xxx"

## 后果

### 好
- 防止长期 wheel 垃圾堆
- 用户不再"两个功能一样的东西选哪个"
- AI 重新校准对 VSF 原生能力的认识

### 坏
- 每个新 wheel 多 5-10 分钟前置工作
- 有时 VSF 原生太复杂（如 PLOOC 宏）我们还是要套壳 → README 说清即可

## 何时应重新评估

- 发生"AI 又重造了 XXX" 事件 → 加强机制
- 有 wheel 因为"套壳 VSF"成为负担 → 允许直接用 VSF 原生，文档化即可

## 相关

- ADR-001（VSF 为基座，所以才有"原生优先"的意义）
- `docs/wheel-contract.md` §1
