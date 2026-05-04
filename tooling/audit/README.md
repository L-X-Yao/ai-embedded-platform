# 审计系统完整方案

本目录包含 ai-embedded-platform 的完整自动化审计系统，覆盖 143 个 wheel 的结构合规、代码安全、测试质量、硬件交互四个维度。

## 设计理念

### 为什么需要审计系统

本项目的核心模式是"人 10% + AI 90%"——AI 负责绝大部分代码实现。这意味着：

- **代码产出速度极快**（一个 session 可以写 40+ 个 wheel）
- **但 AI 不保证正确性**（会写出能编译、能过基本测试、但有隐藏 UB 的代码）
- **人工审查跟不上产出速度**（143 个 wheel × 每个 100-500 行 = 无法逐行审阅）

审计系统的定位是**用自动化工具替代人工审阅的大部分工作**，让人只需要审阅工具无法覆盖的部分（逻辑正确性、需求匹配、架构决策）。

### 核心架构：多层防线

```
越往上越精确、越慢、覆盖越窄
越往下越粗糙、越快、覆盖越广

┌─────────────────────────────────┐
│ 第 9 层：形式化验证（CBMC）      │  数学证明（2 wheels）
├─────────────────────────────────┤
│ 第 8 层：硬件寄存器模型          │  datasheet 验证（1 driver）
├─────────────────────────────────┤
│ 第 7 层：变异测试                │  测试质量度量（10 wheels 基线）
├─────────────────────────────────┤
│ 第 6 层：编译器分析              │  类型安全（111 wheels）
├─────────────────────────────────┤
│ 第 5 层：Sanitizer              │  运行时内存/UB（117 wheels）
├─────────────────────────────────┤
│ 第 4 层：Sentinel 边界测试       │  越界写检测（27 wheels）
├─────────────────────────────────┤
│ 第 3 层：NULL 参数测试           │  NULL 处理（141 wheels）
├─────────────────────────────────┤
│ 第 2 层：属性测试                │  不变式验证（137 wheels）
├─────────────────────────────────┤
│ 第 1 层：单元测试 + 规范检查      │  基本正确性（143 wheels）
└─────────────────────────────────┘
```

**设计原则**：每层只做一件事，每层抓到的 bug 类型和其他层不重叠。全量审计时从第 1 层跑到第 9 层，任何一层失败即拦截。

### 架构决策

**1. 正则 vs 编译器**

audit_wheels.py 的 57 项检查用正则匹配，适合结构/规范类检查（"有没有这个节"、"命名对不对"）。代码安全类检查（"有没有签名左移 UB"）正则天花板低，由 audit_compile.py 和 audit_sanitizer.py 用编译器/运行时替代。

两种工具并存：正则检查快（全量 143 wheels < 1 秒），编译器检查精确但慢（需 cmake 构建）。日常用正则守卫，发布前用编译器扫全量。

**2. 生成 vs 手写测试**

| 方式 | 适用 | 工具 |
|------|------|------|
| 自动生成 | NULL 参数、sentinel 边界 | gen_null_tests.py / gen_sentinel_tests.py |
| 手写 | 单元测试、属性测试、CBMC 验证 | 人/AI 针对 wheel 语义编写 |
| 框架辅助 | 属性测试随机输入、故障注入 | property_test.h / fault_inject.h |

自动生成覆盖**机械性**检查（每个指针参数传 NULL、每个输出缓冲区检查边界），手写覆盖**语义性**检查（编解码对称性、状态机不变式）。

**3. mock vs 硬件模型**

| 方式 | 验证什么 | 精度 | 用途 |
|------|---------|------|------|
| mock_i2c.h | "驱动调了正确的寄存器序列" | 低 | 基本功能测试 |
| hw_model.h | "寄存器操作的硬件行为与 datasheet 一致" | 高 | 驱动 vs datasheet 验证 |

mock 是"录制-回放"，不验证操作正确性。模型是"模拟硬件"，驱动不知道自己在和模型通信——如果写错寄存器地址或位域，模型的行为会不同，测试会失败。

**4. known_exceptions 机制**

Warning 基线必须为 0。任何 warning 要么修复，要么经审查后加入 known_exceptions.json（需写明理由和审批人）。这避免了 warning 疲劳——看到 warning 就是新问题，不需要在一堆老 warning 里找新的。

**5. 快照与趋势追踪**

`--save` 保存每次审计结果为 JSON 快照，`--compare` 对比最近两次。用于回答"这次改动引入了新 warning 吗"和"平台质量在好转还是恶化"。

### 可信度评估

经过全量审计后，系统对每种 bug 类型的捕获能力：

| 维度 | 可信度 | 主要守卫 |
|------|--------|---------|
| 结构/规范合规 | ~99% | audit_wheels.py 57 项检查 |
| 类型安全 | ~95% | audit_compile.py 11 个 -W 标志 |
| 内存安全（已测路径） | ~95% | audit_sanitizer.py ASan+UBSan |
| 输入鲁棒性 | ~90% | 属性测试 + NULL 测试 |
| 边界正确性 | ~85% | sentinel 测试 + 变异测试 |
| 逻辑正确性 | ~40% | 单元测试 + 属性测试（无形式化验证全覆盖） |
| 硬件交互正确性 | ~20% | 仅 bmp280 有寄存器模型 |

**综合可信度约 80%**。剩余 20% 在逻辑正确性（需 Frama-C 形式化验证）和硬件交互（需更多寄存器模型），计划在真实项目开发中逐步补齐。

### 自身可信度

审计工具自身经过验证：

- audit_wheels.py：5 轮 meta-audit + 49 个自测试
- 新工具（8 个）：全部跑过验证，发现并修复了 fault_inject.h off-by-one、audit_sanitizer.py 构建命令等 bug
- 全量审计发现 4 个真实 bug（i2c_scan 越界、fixed_point UB、bme680 UB、fault_inject off-by-one）

---

## 快速上手

```bash
# 日常守卫（每次 push 前）—— 30 秒
make check

# 增强守卫（含编译器分析 + sanitizer）—— 需 WSL
make check-full

# 查看完整审计报告
make audit-report
```

## 工具清单

### 核心审计工具

| 命令 | 工具 | 作用 | 平台 |
|------|------|------|------|
| `make audit` | `audit_wheels.py` | 57 项结构/规范/代码检查 | 全平台 |
| `make audit-compile` | `audit_compile.py` | gcc 11 个 -W 标志编译分析 | 全平台 |
| `make audit-sanitizer` | `audit_sanitizer.py` | AddressSanitizer + UBSan 运行时检测 | WSL |
| `make audit-coverage` | `audit_coverage.py` | gcov 行覆盖率（L0 ≥ 80%, L1-L3 ≥ 60%） | WSL |
| `make audit-deep` | `audit_deep.sh` | 6 层深度分析（grep/sanitizer/fuzz/stress/consistency/cppcheck） | WSL |
| `make audit-quick` | `audit_deep.sh --quick` | 仅 L1+L2（最快的深度检查） | WSL |

### 辅助工具

| 命令 | 工具 | 作用 |
|------|------|------|
| `make audit-report` | `audit_report.sh` | 合并 5 个工具的统一报告 |
| `make audit-fix` | `audit_fix.py` | 机械性问题自动修复（dry-run 预览） |
| `make audit-fix-apply` | `audit_fix.py --apply` | 应用自动修复 |
| `make audit-save` | `audit_wheels.py --save` | 保存审计快照 |
| `make audit-compare` | `audit_wheels.py --compare` | 对比最近两次快照 |
| `make arm-size-audit` | `arm_size_audit.sh` | ARM Cortex-M4 交叉编译体积报告 |

### 测试质量工具

| 命令 | 工具 | 作用 |
|------|------|------|
| `make test-property` | 137 个 `prop_*.c` | 属性测试（~400 万随机输入） |
| `make test-mutate` | `test_mutation.sh` | 变异测试（15 算子，度量杀死率） |
| `make verify` | `verify_*.c` + CBMC | 有界模型检查（需安装 cbmc） |

## 文件结构

```
tooling/
├── audit/                          审计工具
│   ├── _audit_build_base.py        共享构建模块（wheel 发现/cmake/清理）
│   ├── audit_wheels.py             核心：57 项 CHECK_TABLE 检查
│   ├── audit_compile.py            编译器警告分析
│   ├── audit_sanitizer.py          ASan + UBSan
│   ├── audit_coverage.py           gcov 覆盖率
│   ├── audit_deep.sh               6 层深度分析
│   ├── audit_report.sh             统一报告
│   ├── audit_fix.py                机械修复
│   ├── doc_drift_check.py          文档数字同步
│   ├── memory_audit.py             记忆文件健康度
│   ├── arm_size_audit.sh           ARM 体积报告
│   ├── known_exceptions.json       已知例外（含理由和审批人）
│   ├── test_audit_wheels.py        49 个审计引擎自测试
│   ├── README.md                   本文件
│   └── snapshots/                  审计快照（JSON，自动保留最近 20 个）
│
├── test/                           测试框架
│   ├── test_harness.h              统一测试宏（TEST / RUN / ASSERT_*）
│   ├── property_test.h             属性测试（PROPERTY / ARBI_* / PROP_ASSERT_*）
│   ├── contract.h                  契约宏（REQUIRE / ENSURE / INVARIANT）
│   ├── fault_inject.h              故障注入（FAULT_CHECK / fail_after/at/next）
│   ├── hw_model.h                  硬件寄存器模型框架（256 寄存器 + I2C/SPI）
│   ├── mock_i2c.h                  I2C 简单 mock（寄存器数组 + 失败注入）
│   ├── mock_spi.h                  SPI 简单 mock
│   ├── mock_gpio.h                 GPIO 简单 mock
│   ├── gen_null_tests.py           NULL 参数测试生成器
│   ├── gen_sentinel_tests.py       边界 sentinel 测试生成器
│   └── test_mutation.sh            变异测试脚本
│
wheels/<layer>/<name>/test/         每个 wheel 的测试目录
├── test_<name>.c                   单元测试（手写）
├── prop_<name>.c                   属性测试（手写，使用 property_test.h）
├── test_null_<name>.c              NULL 参数测试（gen_null_tests.py 自动生成）
├── test_sentinel_<name>.c          边界测试（gen_sentinel_tests.py 自动生成）
├── verify_<name>.c                 CBMC 形式化验证（手写，仅核心 L0）
└── hw_model_<name>.h               硬件寄存器模型（手写，仅 L1 驱动）

docs/
├── audit-dimensions.md             40 个审计维度定义 + 超级审计执行计划
├── audit-checklist.md              AI agent 审计角度菜单 + 标准边界追踪
└── known-issues.md                 已知问题（超级审计不重复报告）
```

## audit_wheels.py 内部架构

### CHECK_TABLE 表驱动

所有 57 项检查注册在 `CHECK_TABLE` 列表中，每项是一个元组：

```python
CHECK_TABLE = [
    # (name,  heading,  chk_function,  level,  layers,  skip_vendor,  skip_draft)
    ("ci_job", "CI job", chk_ci_job, "FAIL", None, False, False),
    ...
]
```

`main()` 用一个统一循环遍历所有检查和所有 wheel，约 170 行（从重构前的 520 行缩减）。

### WheelCtx 文件缓存

每个 wheel 的 4 个关键文件（.h / .c / README.md / test/CMakeLists.txt）在审计开始时一次性读入 `WheelCtx` 对象，后续 57 项检查共享缓存，消除 ~6000 次重复 IO。全量 143 wheels 审计耗时 < 1 秒。

### 特殊检查（不在 CHECK_TABLE 中）

| 检查 | 原因 |
|------|------|
| consumer | 需要扫描 projects/ 目录，不是 per-wheel 检查 |
| wishlist_stale | 需要读 WISHLIST.md，跨 wheel 比较 |
| project_hygiene | 扫描 projects/ 目录，不在 wheels/ 范围内 |

### 检查分类详解

**结构合规（16 项）**：

| 检查 | 级别 | 说明 |
|------|------|------|
| ci_job | FAIL | wheel 路径必须出现在 CI 配置中 |
| vendor_md | FAIL | vendor wheel 必须有 VENDOR.md |
| cross_wheel_deps | WARN | 跨 wheel 依赖需在 README Dependencies 节文档化 |
| api_review | WARN | 非 draft wheel 需有 API 审批元数据 |
| ctest_registration | WARN | test/CMakeLists.txt 需注册 ctest |
| c99_standard | WARN | CMAKE_C_STANDARD 必须设为 99 |
| candidate_sections | WARN | README 需有 Supported platforms / Test status / Dependencies |
| file_header | WARN | .h/.c 需有 Layer/Status/Deps 文件头 |
| extern_c_guard | WARN | .h 需有 `extern "C"` 守卫 |
| test_exit_code | WARN | 测试需返回 exit code |
| test_run_balance | WARN | TEST 定义数 ≈ RUN 调用数 |
| werror | WARN | CMakeLists 需有 -Werror |
| wconversion | WARN | CMakeLists 需有 -Wconversion |
| responsible_human | WARN | README 需有 Responsible human 节 |
| deviation_heading | WARN | 需有 API convention deviations 节 |
| dep_direction | WARN | 依赖只能向下（L0←L1←L2←L3），ADR-009 |

**API 规范（15 项）**：

| 检查 | 说明 |
|------|------|
| error_code_sign | 错误码必须是显式负数 |
| function_doc | 每个公共函数需有文档注释 |
| error_code_coverage | 每个错误码至少被一个测试触发 |
| naming_convention | 函数命名 `wheel_verb` 模式 |
| hal_injection | L1 驱动需有 HAL 回调注入结构 |
| dev_t_naming | context 类型用 `_t` 不用 `_dev_t` |
| err_plural | 错误码单数（ERR_PARAM 不是 ERR_PARAMS） |
| pragma_once | 用 `#ifndef` 守卫不用 `#pragma once` |
| spdx_header | 需有 SPDX-License-Identifier 行 |
| vsf_native_residual | 不应残留 "Why not VSF native?" 节 |
| init_thread_safe | init/deinit 应标 `@thread_safe: no` |
| deps_format | 文件头 Deps 行格式正确 |
| why_self_written | 非 vendor wheel 需有 "Why self-written?" 节 |
| status_format | **Status:** 用 title case |
| instances_field | 文件头需有 Instances: multi/single |

**代码安全（16 项）**：

| 检查 | 说明 |
|------|------|
| thread_safe | 公共函数需有 `@thread_safe` 标注 |
| isr_safe | L0+L1 公共函数需有 `@isr_safe` 标注 |
| busy_wait_timeout | 忙等循环需有超时守卫 |
| config_h_placement | config.h 应在 .c 中 include，不在 .h |
| nan_guard | 浮点范围检查需用 NaN 安全模式 |
| deinit_safety | deinit 需检查 NULL |
| signed_left_shift | 检测签名左移 UB |
| float_to_int_cast | 浮点转整需有范围检查 |
| strlen_truncation | strlen 结果不应截断到窄类型 |
| memcpy_overflow | memcpy 大小参数不应有 size_t 溢出风险 |
| unchecked_return | 公共 API 返回值需检查或显式 `(void)` 忽略 |
| header_self_contained | .h 需 include 自己用到的标准头 |
| dead_static_func | 检测定义但从未调用的 static 函数 |
| switch_default | switch 需有 default 分支 |
| unit_suffix | L1 驱动物理量字段需有单位后缀 |

**一致性（10 项）**：

| 检查 | 说明 |
|------|------|
| readme_section_order | README 节按标准顺序排列 |
| readme_section_names | 节名使用标准英文名 |
| readme_todo_mismatch | README "Known issues" 与代码 TODO/FIXME 一致 |
| instances_accuracy | Instances 字段与实际代码一致 |

## 测试层次详解

从低到高，每层抓到不同类型的 bug：

```
第 1 层：单元测试（test_*.c）
  → "给这个输入，输出对不对？"
  → 146+ 个，覆盖所有 wheel
  → 框架：test_harness.h（TEST / RUN / ASSERT_*）

第 2 层：属性测试（prop_*.c）
  → "对任意随机输入，不变式是否成立？"
  → 137 个，~400 万次随机输入
  → 框架：property_test.h（PROPERTY / ARBI_* / PROP_ASSERT_*）
  → 发现了 i2c_scan count 越界读 bug

第 3 层：NULL 参数测试（test_null_*.c）
  → "传 NULL 会不会 crash？返回值对不对？"
  → 1318 个测试，141 个 wheel
  → 生成器：gen_null_tests.py

第 4 层：Sentinel 边界测试（test_sentinel_*.c）
  → "输出函数会不会多写一个字节？"
  → 87 个测试，27 个 L0+L2 wheel
  → 生成器：gen_sentinel_tests.py

第 5 层：Sanitizer（audit_sanitizer.py）
  → "运行时有没有内存越界、未定义行为？"
  → 117/141 通过（23 跳过是跨 wheel 依赖）
  → 发现了 fixed_point 签名左移 UB + bme680 5 处 UB

第 6 层：编译器分析（audit_compile.py）
  → "编译器最严格的 -W 标志能不能通过？"
  → 111/141 通过，0 个 warning

第 7 层：变异测试（test_mutation.sh）
  → "改一行代码，测试能不能抓到？"
  → 10 个核心 wheel 基线，平均 ~77% 杀死率（去等价变异 ~85-90%）
  → ring_buffer 手工优化后达到 87%（有效 100%）

第 8 层：硬件寄存器模型（hw_model_*.h）
  → "驱动操作的寄存器序列对不对（vs datasheet）？"
  → 1 个范例（bmp280，14 tests 全绿）
  → 框架：hw_model.h（256 寄存器 + reset/readonly/mask/callback）

第 9 层：CBMC 形式化验证（verify_*.c）
  → "对所有可能输入，不变式是否数学上成立？"
  → 2 个范例（ring_buffer 4 证明 + state_machine 4 证明）
  → 需要安装 cbmc
```

## 如何添加新检查

### 给 audit_wheels.py 加检查

1. 在 CHECK_TABLE 之前写 `chk_*` 函数：

```python
def chk_my_check(ctx, **_):
    """一行描述。"""
    if not ctx.header:
        return None
    if "问题模式" in ctx.header:
        return ("发现了什么问题", "怎么修")
    return None
```

2. 加入 CHECK_TABLE：

```python
("my_check", "检查标题", chk_my_check, "WARN", None, True, False),
#  name      heading     function     level  layers skip_vendor skip_draft
```

3. 在 `test_audit_wheels.py` 加测试（pass + fail 至少各一个）

4. 在真实 143 wheels 上跑一遍验证误报率（**< 30% 才上线**，见 memory: feedback_new_check_fp_validation.md）

### 给新 wheel 加属性测试

```c
#include "test_harness.h"
#include "property_test.h"
#include "../my_wheel.h"

PROPERTY(roundtrip, 10000) {
    uint8_t input[64];
    size_t len = ARBI_RANGE(seed, 1, 64);
    ARBI_FILL(seed, input, len);
    // ... encode → decode → 比较 ...
    PROP_ASSERT_MEM_EQ(decoded, input, len);
}

PROPERTY(no_crash, 50000) {
    uint8_t garbage[256];
    size_t len = ARBI_RANGE(seed, 0, 256);
    ARBI_FILL(seed, garbage, len);
    my_wheel_parse(garbage, len);  // 不 crash 就是 pass
}

TEST_MAIN("my_wheel properties") { RUN(roundtrip); RUN(no_crash); } TEST_MAIN_END()
```

### 给 L1 驱动加硬件模型

参照 `wheels/drivers/bmp280/test/hw_model_bmp280.h`：

1. 从 datasheet 提取寄存器表（地址、复位值、读写属性）
2. 用 `hw_model.h` 框架建模
3. 为 ctrl/config 寄存器加 `on_write` 回调模拟硬件行为
4. 写测试验证驱动操作 vs 模型行为一致

### 批量生成 NULL / Sentinel 测试

```bash
# 重新生成所有 NULL 参数测试（覆盖已有文件）
python3 tooling/test/gen_null_tests.py

# 重新生成所有 sentinel 边界测试
python3 tooling/test/gen_sentinel_tests.py
```

## 全量审计流程

```bash
# Phase 0：自动化守卫（确认基线干净）
make check                     # 0 failures + 0 warnings

# Phase 1：编译器 + Sanitizer（WSL 中运行）
make audit-compile             # 0 warnings
wsl python3 tooling/audit/audit_sanitizer.py   # 0 failures

# Phase 2：属性 + NULL + Sentinel 测试
make test-property             # 全绿
# NULL 和 sentinel 测试需手动编译（尚未集成进 test_all.sh）

# Phase 3：深度分析（WSL 中运行）
make audit-deep                # 6 层全跑

# Phase 4：变异测试（度量测试质量）
make test-mutate               # 目标 ≥ 85% 杀死率

# Phase 5（可选）：超级审计
# 用 dispatch_agent.sh 派发 AI agent 审阅 40 个维度
# 详见 docs/audit-dimensions.md
```

## 演进路线

### 已完成

- ✅ 57 项 CHECK_TABLE 规范检查（v3，5 轮 meta-audit）
- ✅ 编译器分析 / Sanitizer / 覆盖率工具（v4）
- ✅ 属性测试框架 + 137 个 wheel 覆盖（v4）
- ✅ NULL / Sentinel 测试生成器（v4）
- ✅ 变异测试基线（v4）
- ✅ 硬件寄存器模型范例：bmp280（v4）
- ✅ CBMC 验证范例：ring_buffer + state_machine（v4）
- ✅ 契约框架：contract.h + ring_buffer 应用（v4）
- ✅ warning 基线归零（v4）
- ✅ 工具自身 meta-audit + bug 修复（v4）
- ✅ 全量审计（v4，发现并修复 4 个真实 bug）

### 未来方向（在真实项目开发中逐步推进）

| 方向 | 预期收益 |
|------|---------|
| 更多 L1 驱动的硬件寄存器模型 | 硬件正确性 20% → 50%+ |
| 更多 L0 核心的 CBMC 验证 | 逻辑正确性数学保证 |
| 全部 wheel 添加 REQUIRE/ENSURE 契约 | 运行时不变式检查 |
| Frama-C + ACSL 形式化规范 | 逻辑正确性 40% → 70%+ |
| 差分测试（对比参考实现） | L2 协议正确性验证 |

## 当前审计状态

```
audit_wheels.py:   0 failures, 0 warnings (3 known exceptions)
audit_compile.py:  111/141 pass, 0 warnings
audit_sanitizer:   117/141 pass, 0 failures (WSL)
属性测试:          135/137 pass
NULL 测试:         140/141 pass
Sentinel 测试:     26/27 pass
变异测试基线:      ~77% 平均杀死率（去等价变异 ~85-90%）

全量审计发现并修复的 bug：
1. i2c_scan count 溢出（越界读）—— 属性测试发现
2. fixed_point 签名左移 UB —— sanitizer 发现
3. bme680 5 处 UB（签名左移 + 整数溢出）—— sanitizer 发现
4. fault_inject.h off-by-one —— meta-audit 发现
```
