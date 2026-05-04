# 审计系统使用指南

本目录包含 ai-embedded-platform 的完整自动化审计系统，覆盖 143 个 wheel 的结构合规、代码安全、测试质量、硬件交互四个维度。

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
| `make test-mutate` | `mutation_test.sh` | 变异测试（15 算子，度量杀死率） |
| `make verify` | `verify_*.c` + CBMC | 有界模型检查（需安装 cbmc） |

## 文件结构

```
tooling/
├── audit/                          审计工具
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
│   ├── known_exceptions.json       已知例外（含理由）
│   ├── test_audit_wheels.py        49 个审计引擎自测试
│   └── snapshots/                  审计快照（JSON）
│
├── test/                           测试框架
│   ├── test_harness.h              统一测试宏（TEST / RUN / ASSERT_*）
│   ├── property_test.h             属性测试（PROPERTY / ARBI_* / PROP_ASSERT_*）
│   ├── contract.h                  契约宏（REQUIRE / ENSURE / INVARIANT）
│   ├── fault_inject.h              故障注入（FAULT_CHECK / fail_after/at/next）
│   ├── hw_model.h                  硬件寄存器模型框架
│   ├── mock_i2c.h                  I2C 简单 mock
│   ├── mock_spi.h                  SPI 简单 mock
│   ├── mock_gpio.h                 GPIO 简单 mock
│   ├── gen_null_tests.py           NULL 参数测试生成器
│   ├── gen_sentinel_tests.py       边界 sentinel 测试生成器
│   └── mutation_test.sh            变异测试脚本
│
wheels/<layer>/<name>/test/         每个 wheel 的测试目录
├── test_<name>.c                   单元测试（手写）
├── prop_<name>.c                   属性测试（生成或手写）
├── test_null_<name>.c              NULL 参数测试（gen_null_tests.py 生成）
├── test_sentinel_<name>.c          边界测试（gen_sentinel_tests.py 生成）
├── verify_<name>.c                 CBMC 形式化验证（手写）
└── hw_model_<name>.h               硬件寄存器模型（手写，仅 L1）
```

## audit_wheels.py 57 项检查详解

### 检查分类

| 类别 | 数量 | 举例 |
|------|------|------|
| 结构合规 | 16 | CI 注册、README 节、编译标志、extern C、依赖方向 |
| API 规范 | 15 | 命名约定、错误码、线程安全标注、SPDX 许可证 |
| 代码安全 | 16 | 签名左移 UB、浮点转整、strlen 截断、死代码、switch default |
| 一致性 | 10 | README 节顺序/命名、TODO 匹配、Instances 准确性 |

### known_exceptions.json

当一个 warning 经过验证确认不需要修复时，添加到此文件：

```json
{
  "exceptions": [
    {
      "wheel": "wheels/drivers/nrf24l01",
      "check": "switch_default",
      "reason": "enum switch covers all 3 values; default would suppress -Wswitch-enum",
      "added": "2026-05-04",
      "reviewer": "human"
    }
  ]
}
```

被 exception 的 warning 自动降级为 INFO，不计入 warning 总数。

## 测试层次

从低到高，每层抓到不同类型的 bug：

```
第 1 层：单元测试（test_*.c）
  → "给这个输入，输出对不对？"
  → 146+ 个，覆盖所有 wheel

第 2 层：属性测试（prop_*.c）
  → "对任意随机输入，不变式是否成立？"
  → 137 个，~400 万次随机输入
  → 发现了 i2c_scan count 越界读 bug

第 3 层：NULL 参数测试（test_null_*.c）
  → "传 NULL 会不会 crash？返回值对不对？"
  → 1318 个测试，141 个 wheel

第 4 层：边界 sentinel 测试（test_sentinel_*.c）
  → "输出函数会不会多写一个字节？"
  → 87 个测试，27 个 L0+L2 wheel

第 5 层：Sanitizer（audit_sanitizer.py）
  → "运行时有没有内存越界、未定义行为？"
  → 117 个 wheel 通过
  → 发现了 fixed_point 签名左移 UB + bme680 5 处 UB

第 6 层：编译器分析（audit_compile.py）
  → "编译器最严格的 -W 标志能不能通过？"
  → 111 个 wheel，0 个 warning

第 7 层：变异测试（mutation_test.sh）
  → "改一行代码，测试能不能抓到？"
  → 10 个核心 wheel 基线，平均 ~77% 杀死率

第 8 层：硬件寄存器模型（hw_model_*.h）
  → "驱动操作的寄存器序列对不对（vs datasheet）？"
  → 1 个范例（bmp280，14 tests）

第 9 层：CBMC 形式化验证（verify_*.c）
  → "对所有可能输入，不变式是否数学上成立？"
  → 2 个范例（ring_buffer + state_machine）
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

4. 在真实 143 wheels 上跑一遍验证误报率（< 30% 才上线）

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

## 全量审计流程

```bash
# Phase 0：自动化守卫（确认基线干净）
make check                     # 0 failures + 0 warnings

# Phase 1：编译器 + Sanitizer
make audit-compile             # 0 warnings
wsl make audit-sanitizer       # 0 failures

# Phase 2：属性 + NULL + Sentinel 测试
make test-property             # 全绿
# NULL 和 sentinel 测试需手动编译（尚未集成进 test_all.sh）

# Phase 3：深度分析
make audit-deep                # 6 层全跑

# Phase 4：变异测试（度量测试质量）
make test-mutate               # 目标 ≥ 85% 杀死率

# Phase 5（可选）：超级审计
# 用 dispatch_agent.sh 派发 AI agent 审阅 40 个维度
# 详见 docs/audit-dimensions.md
```

## 当前审计状态

```
audit_wheels.py:   0 failures, 0 warnings (3 known exceptions)
audit_compile.py:  111/141 pass, 0 warnings
audit_sanitizer:   117/141 pass, 0 failures
属性测试:          135/137 pass
NULL 测试:         140/141 pass
Sentinel 测试:     26/27 pass
变异测试基线:      ~77% 平均杀死率（去等价变异 ~85-90%）

全量审计发现并修复的 bug：
1. i2c_scan count 溢出（越界读）—— 属性测试发现
2. fixed_point 签名左移 UB —— sanitizer 发现
3. bme680 5 处 UB —— sanitizer 发现
4. fault_inject.h off-by-one —— meta-audit 发现
```
