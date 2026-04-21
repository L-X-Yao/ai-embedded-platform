# wheels/_draft/ — 孵化区

新 wheel 初次创建时放这里。达到 wheel-contract draft 级要求后，移到对应层（`primitives/` / `drivers/` / `protocols/` / `modules/`）。

## 从 _draft 升级到正式层的检查清单

- [ ] sim 能跑通
- [ ] 至少 3 个测试 case（正常路径 / 边界 / 错误输入）
- [ ] 有最小 README：API 头 + "how to test" 5 行说明
- [ ] 有 "Why not VSF native?" 节说明为何自造而非用 VSF 原生
- [ ] API 设计经过**人工审批**（不能 AI 自己决定）

满足后移到：
- L0 纯软件工具 → `wheels/primitives/`
- L1 硬件驱动 → `wheels/drivers/`
- L2 协议/服务 → `wheels/protocols/`
- L3 功能模块 → `wheels/modules/`

见 [`docs/wheel-contract.md`](../../docs/wheel-contract.md) 了解完整合格定义。

## 当前孵化中

（暂无）
