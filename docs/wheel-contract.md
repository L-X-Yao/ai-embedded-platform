# 轮子合格定义（Wheel Contract）

**定位**：这是**每个 wheel 的质量门清单**——单个 wheel 能不能从 draft 过审到 candidate/stable 的检查表。

- 这个文档回答"一个 wheel 过不过审"
- `api-conventions.md` 回答"API 怎么写才合规"

一个目录要被放进 `wheels/<层>/`（非 `_draft/`），必须满足本文档的强制项；真机验收和 API 审批是升 stable 的条件（见 §0）。没满足就还是草稿。

## 0. 三级门槛（按状态）

| 状态 | 位置 | 文档/测试要求 |
|------|------|--------------|
| **draft** | `wheels/_draft/<name>/` | 最小：API 头 + 测试能跑 + `README.md` 只需"how to test"5 行 |
| **candidate** | `wheels/<层>/<name>/` + `Status: candidate` | 主机验证全部通过：结构、README 章节、主机测试、审计、sanitizer、覆盖率门禁（`make wheel-gate` 和 `make check`）。不要求真机，也不要求 API 审批记录 |
| **stable** | `wheels/<层>/<name>/` + `Status: stable` | 全部强制 + 推荐项 + 真机验收记录（L1、L3）+ API 审批记录 + 至少 2 个真项目用过 |

**`_draft/` 下为 draft；放进 `wheels/<层>/` 即为 candidate，提交前必须过 `make wheel-gate`**。升 stable 需人审批。真机验收和 API 审批做到哪一步，如实写在 README 的 `Supported platforms` 和 `API review` 节，没做就写没做。

2026-11-11 起按此定义（超级审计 D1-5）。原定义要求 candidate 已有真机验收和 API 审批记录，但 259 个 wheel 都标了 candidate，115 个驱动只有 4 个、20 个模块一个也没有真机记录，所以改定义，不改标签。

### 0.1 自动化护栏

`tooling/audit/audit_wheels.py` 在 `wheel-audit` CI job 里强制以下规则——push 阶段直接 fail，不靠人脑或 review checklist 维护：

- 每个 `wheels/<层>/<name>/` **必须**在 `.github/workflows/ci.yml` 有对应 build + test job（FAIL）
- `drivers/` / `protocols/` / `modules/` 层的 wheel **应该**有项目 consumer（CMakeLists.txt 或 #include）；缺失为 WARN 不阻塞——AI 主动构建是鼓励的（见 ADR-009）
- `primitives/` 和 `_draft/` 层可以暂无 consumer（作为 building block 或孵化品）
- `wheels/WISHLIST.md` 不能列已存在的 wheel 名（stale candidate）
- ~~每个 wheel 的 README 必须有 `## Why not VSF native?` 标题~~ （ADR-006 已被 ADR-011 supersede）
- Vendor wheel（含 `vendor/` 子目录）**必须**有 `VENDOR.md`（ADR-010，FAIL）
- 有跨 wheel 依赖的 wheel **应该**在 README 写 `## Dependencies` 节（ADR-009，WARN）
- 非 `_draft/` 层的 wheel **应该**在 README 有 `## API review` 节（wheel-contract §7，WARN）
- 每个 wheel 的 test CMakeLists.txt **应该**有 `enable_testing()` + `add_test()`（ctest 兼容，WARN）
- 每个 wheel 的 test CMakeLists.txt **应该**设置 `CMAKE_C_STANDARD 99`（构建一致性，WARN）
- 非 vendor 的 wheel `.h`/`.c` **应该**有 `Layer:`/`Status:`/`Deps:`/`Instances:` 文件头（api-conventions §8，WARN）
- 非 vendor 的 wheel `.h` **应该**有 `extern "C"` 守卫（api-conventions §9，WARN）
- 非 vendor 的 wheel `.h` **不应**使用 `#pragma once`（api-conventions §9.2，WARN）
- 非 `_draft/` 层的 wheel README **应该**有 `## Supported platforms` + `## Test status` + `## Dependencies` 节（wheel-contract §3.2 候选级，WARN）
- 非 vendor 的 wheel `.h`/`.c` **应该**有 SPDX 许可证标识（api-conventions §8.3，WARN）
- `@thread_safe` 标注：`init`/`deinit`/`reset` 不应标 `caller_must_lock`（api-conventions §4，WARN）
- 主 context 类型命名应为 `<name>_t`，不应为 `<name>_dev_t`（api-conventions §1.3，WARN）
- 错误码命名应为单数 `ERR_PARAM`，不应为复数 `ERR_PARAMS`（api-conventions §2.2，WARN）
- Deps 格式：接口依赖用 `<name>_hal (injected)`，具体依赖用 `<name>`，无依赖用 `(none)`（api-conventions §8.1，WARN）
- "Why not VSF native?" 残留检测：ADR-006 已废弃，不应再出现（WARN）

历史教训：2026-04-25/26 曾出现 2 个 orphan wheel（commit 进 repo 但既没 CI 也没 consumer，持续 1-2 天才被发现），audit 工具就是为消除这类 systemic 漂移而生。

## 1. 核心原则：Wheel 是被动库

完整架构见 **[docs/architecture.md](architecture.md)**。

**本文关心的验收点**：

- Wheel 不创建任务、不加锁、不 sleep、不分配堆内存、不 `#include` OS 头文件
- 依赖通过函数指针注入（HAL 回调）
- 新建 wheel 前查通用开源实现（ADR-010），能 vendor 就 vendor
- ~~ADR-006 "grep VSF" 已被 ADR-011 supersede~~——wheel 独立于 OS，VSF 有无不影响决策

## 2. 结构（强制）

### 2.1 自写 wheel（标准结构）

```
wheels/<层>/<wheel-name>/
├── README.md                 必须
├── <wheel-name>.h            必须（public API）
├── <wheel-name>.c            必须
├── test/                     必须
│   ├── CMakeLists.txt        必须（独立可构建）
│   └── test_<wheel-name>.c   必须（C 测试 harness）
├── config.h                  可选（编译期配置）
└── examples/                 可选
```

### 2.2 Vendor wheel（ADR-010 结构）

```
wheels/<层>/<wheel-name>/
├── README.md                 必须（含 "Why this upstream?" 节）
├── VENDOR.md                 必须（upstream URL / 版本 / 更新步骤）
├── vendor/                   必须
│   ├── inc/                  上游 headers（不修改）
│   └── src/                  上游 sources（不修改）
├── test/                     必须
│   ├── CMakeLists.txt        必须（引用 ../vendor/src/）
│   ├── test_<wheel-name>.c   必须
│   └── <xxx>_cfg.h           常有（per-project 配置示例）
└── examples/                 可选
```

脚手架生成：`tooling/wheel/new_wheel.sh --vendor --layer <layer> --name <name>`

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

**最小门槛如此**。但 draft 也**允许**（且推荐）直接写接近 candidate 级的完整 README——设计热度在手时写比事后补便宜（log wheel 就是这样做的）。candidate/stable 的真实门槛是 §3.2/§3.3 内容 + 真实项目使用次数，不由 README 长短把关。

### 3.2 candidate 级完整要求

在 draft 基础上补。**章节顺序固定**（审计脚本检查）：

```markdown
# <wheel-name>

**Status: candidate**
**Layer: L1 (drivers)**

2-3 句功能描述。

## Origin

（来源说明：从哪个项目提取 / 主动构建用于什么场景）

## Why self-written?

Searched GitHub/embedded ecosystem (2026-xx-xx):

- **<lib1>** — <不适合原因>
- No suitable zero-dependency C99 implementation found.

Reference: <参考了哪个实现的算法>

（ADR-010 合规证据。vendor wheel 用 `## Why this upstream?` 代替此节）

## Dependencies

- I2C HAL (injected)
- 其他轮子：crc16（若有）

## API

\`\`\`c
// 关键函数 + 一句话说明
int xxx_init(xxx_t *x, const xxx_cfg_t *cfg);
\`\`\`

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| XXX_MAX_SIZE | 64 | ... |

## Supported platforms

- [x] Host (sim) — passed 2026-xx-xx
- [ ] ARM Cortex-M — not yet verified
（只有在真机上跑过才打勾。L1、L3 升 stable 必须至少有 1 个真机打勾）

## Known issues

- 坑 1：xxx → 解决 yyy

## How to test

\`\`\`bash
cd wheels/<layer>/<name>/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_<name>
\`\`\`

## Test status

- Unit tests (sim): PASS (N tests, M assertions)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-xx-xx
- **Key decisions**: ...

## Responsible human

@user
```

**注意**：
- `**Status:**` 用首字母大写（不是全大写 `STATUS`）
- 章节名用英文（不是中文"功能"/"依赖"/"已知坑"）
- `Configuration (compile-time)` 不缩写为 `Configuration macros` / `Configuration`
- self-written wheel 必须有 `Why self-written?`，vendor wheel 用 `Why this upstream?`
- 额外章节（`Features`、`Usage example`、`Design tradeoffs`）允许添加，但必须放在标准章节之后

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
- **输出格式**：`N passed, M failed`（`test_harness.h` 标准输出，`test_all.sh` 靠退出码判断结果）

**L0**：sim 测试覆盖 ≥ 80% 代码行（目测或用 gcov）  
**L1**：sim mock 测试；升 stable 另需至少 1 次真机验收记录  
**L2**：sim loopback 测试（模拟两端通信）+ 协议一致性测试  
**L3**：集成测试（组合多个 wheel）；升 stable 另需真机验收

## 5. API 设计规则（强制）

**完整规则见 [`docs/api-conventions.md`](api-conventions.md)**（单一来源，不在本文重复）。

**本文关心的验收点**：
- README 或代码中有"Convention Deviations"节——如偏离 api-conventions 需列出并说明理由
- 否则：wheel 必须遵守 api-conventions 全部强制条款

**文档与行为绑定**（强制）：
- 每个公开函数的 header 注释必须描述**完整行为**：输入约束、输出、边界（NULL / 0 / 上限）、错误条件、线程安全
- 测试必须覆盖每个文档描述的行为点——尤其 NULL / 0 / 边界输入
- **文档说了什么、测试必须测什么**。文档与代码漂移被 CI 抓 = 比被用户抓便宜

（反例：log wheel 初版 README 写 "NULL = 关时间戳" 但代码 fallback 到默认；测试 catch 了。这条规则就是从这次经验来的。）

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

**没有 API 审批记录的 wheel 不能升 stable**（还没审批的 candidate 在 README 的 API review 节写明未审批）。审批记录放 `README.md` 底部"API Review"节：

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

- ❌ wheel 内部创建任务/加锁/sleep/malloc → 拒收（见 §1）
- ❌ 没 README 或 README 只一句话（非 draft）
- ❌ 测试全是 `assert True`
- ❌ 代码里有 TODO / FIXME 没处理完
- ❌ 全局单例 + 不可重入
- ❌ 依赖某个具体 MCU 寄存器地址
- ❌ 接口和其他 wheel 风格漂移（不遵循 api-conventions.md）
- ❌ L1+ 声称支持 MCU 但没真机验收记录

## 10. 升级路径

每次升级需人审：

- `_draft/<name>/` → `<层>/<name>/`，`Status: draft` 改为 `candidate`：过 `make wheel-gate` 后 AI 可做，人终审
- `Status: candidate` → `Status: stable`：人确认真机验收记录（L1、L3）、API 审批记录，并验证"至少 2 个真项目使用"后手改

自动化脚本：`tooling/wheel/wheel_promote.sh <wheel-path>` 处理目录移动和引用更新。

**降级**（冷门、API 过时等）由 AI 季度审计提议，人批准。
