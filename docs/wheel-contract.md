# 轮子合格定义（Wheel Contract）

一个目录要被放进 `wheels/<层>/`（非 `_draft/`），必须满足本文档清单。没满足就还是草稿。

## 0. 三级门槛（按状态）

| 状态 | 位置 | 文档/测试要求 |
|------|------|--------------|
| **draft** | `wheels/_draft/<name>/` | 最小：API 头 + 测试能跑 + `README.md` 只需"how to test"5 行 |
| **candidate** | `wheels/<层>/<name>/` + `STATUS: candidate` | 本文档全部强制项满足 |
| **stable** | `wheels/<层>/<name>/` + `STATUS: stable` | 全部强制 + 推荐项 + 至少 2 个真项目用过 |

**AI 新建的 wheel 默认为 draft**。升 candidate / stable 需人审批。

## 1. 零号铁律：Why not VSF native?

**新建 wheel 的第一件事**：检查 VSF 是否已有等价物。

必须在 README 里写一节：

```markdown
## 为什么不用 VSF 原生？

已检查以下 VSF 模块：
- `vsf_xxx`：✓ / ✗（原因）
- `vsf_yyy`：✓ / ✗（原因）

结论：VSF 无等价物 / VSF 有部分等价物但缺 A/B 功能 / VSF 等价物语义不符因 C
```

**如果等价物存在且功能够用 → 不做这个 wheel**，直接文档化"用 vsf_xxx"。

常见等价物速查见 `docs/debugging-playbook.md` → "VSF 数据结构速查表"。

## 2. 结构（强制）

```
wheels/<层>/<wheel-name>/
├── README.md                 必须
├── CMakeLists.txt            必须（能被 add_subdirectory 纳入）
├── <wheel-name>.h            必须（public API）
├── <wheel-name>.c            必须
├── test_<wheel-name>.py      必须（pytest 可跑）
├── test_main.c               常有（C 测试 harness，sim 里跑）
├── config.h                  可选（编译期配置）
└── examples/                 可选
    └── basic.c
```

## 3. README.md 内容（按层差异）

### 3.1 draft 级最小要求

```markdown
# <wheel-name>

**STATUS: draft**

一句话功能。

## How to test

\`\`\`bash
cd wheels/_draft/<wheel-name>
VSF_PATH=/path/to/vsf cmake -GNinja -S . -B build
cmake --build build
python3 -m pytest test_*.py -v
\`\`\`
```

**就这么多**。draft 阶段求快，不求齐全。

### 3.2 candidate 级完整要求

在 draft 基础上补：

```markdown
## 为什么不用 VSF 原生？
（见 §1，强制）

## 功能
- 要点列表

## API

\`\`\`c
// 关键函数 + 一句话说明
int xxx_init(xxx_t *x, const xxx_cfg_t *cfg);
\`\`\`

## 支持的平台
- [x] VSF linux host (sim) — 测试通过 2026-xx-xx
- [ ] VSF + STM32F4 — 未验证
- [ ] VSF + ESP32 — 未验证
（L1+ 必须至少有 1 个真机打勾）

## 依赖
- VSF kernel
- 其他轮子：wheel-xxx（若有）

## 已知坑
- 坑 1：xxx → 解决 yyy

## 测试状态
- 单元测试：PASS
- 硬件在环：N/A / PASS on <板子>, 日期

## 用法示例
（或 link 到 examples/）

## API 约定偏离
（如有不遵守 api-conventions.md 的地方，列出 + 理由）

## Responsible human
@user
```

### 3.3 stable 级额外要求

在 candidate 基础上补：

```markdown
## 性能参考
- 初始化：<cycles>
- 典型操作：<time>
- RAM：<bytes>

## 变更历史
- v0.2.0（2026-xx-xx）：加 X 功能
- v0.1.0（2026-xx-xx）：初版
```

## 4. 测试（强制）

- **能在 host sim 里跑**（VSF linux64 target）
- pytest 能 discover 且所有测试绿
- 至少 **3 个测试 case**：正常路径 / 边界 / 错误输入
- 测试独立可复现（不依赖全局状态、不依赖外部网络）

**L0**：sim 测试覆盖 ≥ 80% 代码行（目测或用 gcov）  
**L1**：sim mock 测试 + 至少 1 次真机验收记录  
**L2**：sim loopback 测试（模拟两端通信）+ 协议一致性测试  
**L3**：集成测试（组合多个 wheel）+ 真机验收

## 5. API 设计规则（强制）

遵守 `docs/api-conventions.md`。关键点：

- 命名前缀 `<wheel>_*`
- 错误码用 `vsf_err_t`
- 返回值 0 成功 / 负数错误
- Context 调用方分配
- 线程安全必须在头文件注释里声明

## 6. 跨平台抽象（强制）

- 涉及硬件走 VSF HAL，**禁止**直接寄存器
- 涉及线程/同步用 VSF kernel 原语（`vsf_eda_*`）
- 字节序、对齐假设必须在 README 说清

**注**：跨 RTOS 可移植性**从强制降为可选推荐**——VSF 是永久基座（见 `docs/adr/007`）。但仍建议接口简洁，不滥用 VSF 内部复杂特性。

## 7. API 设计审批（强制）

新 wheel 的 API（函数签名 + 核心类型）**必须经过人工审批**，流程：

1. AI 产出 API 提案（头文件 + 2 个使用示例）
2. 用户 15 分钟内拍板：通过 / 这样改 / 不抽
3. AI 按拍板后的 API 实现

**没有 API 审批记录的 wheel 不能升 candidate**。审批记录放 `README.md` 底部"API Review"节：

```markdown
## API Review
- 提案日期：2026-xx-xx
- 审批人：@user
- 关键决策：<用户提出的修改点>
```

## 8. 推荐项（不强制但加分）

### 8.1 性能参考（README 里）

### 8.2 `examples/` 目录

### 8.3 Kconfig 或 CMake 开关（默认关）

## 9. 反面清单（不合格的征兆）

- ❌ 没 "Why not VSF native?" 节 → 拒收
- ❌ 没 README 或 README 只一句话（非 draft）
- ❌ 测试全是 `assert True`
- ❌ 代码里有 TODO / FIXME 没处理完
- ❌ 全局单例 + 不可重入
- ❌ 依赖某个具体 MCU 寄存器地址
- ❌ 接口和其他 wheel 风格漂移（不遵循 api-conventions.md）
- ❌ L1+ 声称支持 MCU 但没真机验收记录

## 10. 升级路径

每次升级需人审：

- `_draft/<name>/` → `<层>/<name>/` with `STATUS: draft`：AI 可做，人终审
- `STATUS: draft` → `STATUS: candidate`：API review 通过后人手改
- `STATUS: candidate` → `STATUS: stable`：人验证"至少 2 个真项目使用"后手改

**降级**（冷门、API 过时等）由 AI 季度审计提议，人批准。
