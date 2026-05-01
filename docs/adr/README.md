# Architecture Decision Records (ADR)

每个 ADR 记录一个关键架构/策略决策。用于几个月后或新人加入时回溯"为什么这么设计"。

## 格式约定

文件名：`NNN-<slug-kebab-case>.md`，从 001 递增。

模板：

```markdown
# ADR NNN: <决策标题>

**状态**：proposed / accepted / deprecated / superseded by ADR-XXX
**日期**：YYYY-MM-DD
**决策人**：@username

## 背景
为什么需要这个决策？当时面临什么问题？

## 考虑过的选项
- 选项 A：描述 / 优点 / 缺点
- 选项 B：描述 / 优点 / 缺点
- 选项 C：...

## 决策
选择了哪个选项，一句话总结。

## 后果
- 好：...
- 坏：...

## 何时应重新评估
（列出触发重评的条件，比如"团队规模 > 10 人"、"VSF 不再活跃"）

## 相关
- ADR-XXX（如有）
- 代码/文档引用
```

## 当前 ADR 索引

| # | 决策 | 状态 | 关键结论 |
|---|------|------|---------|
| 001 | 平台选择 VSF | accepted | VSF 为主基座（USB/BLE/GUI/小资源强项） |
| 002 | Wheel 验证采用 sim 优先 | accepted | sim 是第一级验证，HIL 是第二级 |
| 003 | 客户代码回流策略 | accepted | 可回流通用部分，剥离客户 IP |
| 004 | Monorepo 结构 | accepted | 单仓库管理所有内容，不 fork VSF |
| 005 | VSF 上游修改策略 | accepted | patches + vsf-addon，保持 vsf/ 干净 |
| 006 | 优先用 VSF 原生，不重造轮子 | accepted | 新 wheel 前必须 grep VSF |
| 007 | VSF 作为永久基座 | accepted | 不为切 RTOS 做架构预案，AI 兜底维护 |
| 008 | VSF linux64 不是 kernel-only sim 路径 | accepted | kernel-only sim 走 mps2+QEMU，linux64 仅用于 busybox 模式 |
| 009 | Wheel 依赖规则 | accepted | 两种类型（具体/接口）+ 方向只能向下 + 禁止循环 |
| 010 | 优先采纳开源实现 | accepted | 查 VSF → 联网搜开源 → 都没有才从零写；vendor 模式 |
| 011 | Wheel 构建策略：生态驱动 | accepted | 不等项目触发，主动按领域构建；grep VSF 从门禁降为参考 |
