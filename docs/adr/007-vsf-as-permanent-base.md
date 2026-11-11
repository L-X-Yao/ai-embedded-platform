# ADR 007: VSF 作为永久基座

**状态**：accepted
**日期**：2026-04-21
**决策人**：@L-X-Yao

## 背景

早期设计时，我曾担心"VSF 项目死掉"（Simon Qian 停更、bus factor = 2）的风险，倾向于让 wheels 保留"可移植到 NuttX / Zephyr"的抽象层。用户明确否定了这个担心。

## 用户原话

> VSF 肯定不会没了，这个母仓库全力依赖 VSF 作为底座，如果后续原仓库人不继续维护了，我们自己用 AI 进行后续维护就行了。

## 考虑过的选项

### A. 保留跨 RTOS 可移植（wheels 走抽象层）
- 优点：VSF 若死有 fallback
- 缺点：抽象层带来复杂度 / 性能损失 / 代码量膨胀

### B. 全绑 VSF，不留 fallback（选中）✓
- 优点：wheels 可深度利用 VSF 特性、代码简洁
- 缺点：VSF 死掉后接管维护成本高

### C. 混合：L3 模块走抽象，L0-L2 直绑 VSF
- 优点：部分保留切换能力
- 缺点：抽象分层搞不清容易扯皮

## 决策

**选 B：全绑 VSF**。

### 含义

1. wheels 可使用 VSF 任何 API（包括内部）
2. wheels 可选用 PLOOC OOP 风格（不强制，跟周围 VSF 代码一致时用）
3. 不设计 "shim layer" 以切换 RTOS（除非为客户皮肤机制服务）
4. `wheel-contract.md` 里"跨 RTOS 可移植"**从强制降为可选推荐**
5. ADR-001 不再把"VSF 死掉"列为风险

### 若 VSF 真停更怎么办

1. 我们已 clone 全部源码到 monorepo
2. Apache 2.0 允许自由维护
3. 成立私有 fork（迁到公司 GitHub/Gitea）
4. AI + 2-3 人团队维护——VSF 约 700K 行，稳态维护可行
5. 若出现不可逾越的架构问题（如新 MCU 时代完全兼容不了） → 届时重新评估

### 客户指定其他 RTOS 怎么办

走 VSF 皮肤层适配（linux skin 已有 POSIX，其他皮肤可扩）。**wheels 写成 POSIX 风格**可自动适配。**不**是切换 VSF 整个底座。

## 后果

### 好
- 设计简洁、性能好
- wheels 可吃到 VSF 所有特性（如统一 HAL、皮肤、PLOOC OOP）
- 心智模型简单

### 坏
- VSF 真的停更且无法接管 → 全部代码需要大改造
- 不能用同一个 wheel 直接服务 NuttX/Zephyr 项目

### 长期风险容忍度

用户明确接受这个风险。不再设计时考虑切换。

## 何时应重新评估

仅当以下**同时**发生：
- VSF 项目 12 个月无 commit
- 出现我们团队 + AI 接管无法解决的架构问题
- 出现 VSF 强竞争对手（比 NuttX/Zephyr 显著更好的选择）

单独某一条不触发重评估。

## 相关

- ADR-001（VSF 为基座）
- `docs/wheel-contract.md` §6
