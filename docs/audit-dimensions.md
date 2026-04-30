# 平台审计维度清单

本文档定义了 AI 可执行的全部审计维度。用于任务类型 11（平台自省）的标准化执行。

## 交互协议

| 指令 | 行为 |
|------|------|
| `审计` | 列出维度表，等用户选择 |
| `审计 <维度名>` | 单维度审查 + 修复 |
| `超级审计` | 全维度并行启动，按完成顺序汇报 + 修复 |

## 36 个审计维度

### 批次 1 — Wheel 源码

| ID | 维度 | 检查内容 | 范围 |
|----|------|----------|------|
| W1 | **Wheel 代码 (L0)** | 逻辑错误、溢出、UB、API 一致性、线程安全、边界测试缺失 | `wheels/primitives/` 全部 .h/.c/test |
| W2 | **Wheel 代码 (L1)** | 同上 + 寄存器/位掩码 vs datasheet、HAL 注入正确性 | `wheels/drivers/` 全部 |
| W3 | **Wheel 代码 (L2)** | 同上 + 协议合规性、恶意输入安全性 | `wheels/protocols/` 全部 |

### 批次 2 — 基础设施

| ID | 维度 | 检查内容 | 范围 |
|----|------|----------|------|
| T1 | **工具链脚本** | shell 注入、未引号变量、逻辑错误、可移植性 | `tooling/scripts/` + `.githooks/` |
| C1 | **CI + 跨 wheel 一致性** | job 逻辑、缓存、失败模式 + extern C/thread_safe/harness/C99/-Werror/TEST-RUN/VSF-native/include-guard/API-review 8 项规范检查 | `.github/workflows/` + 全部 wheel |
| P1 | **项目代码** | 竞态、栈溢出、Flash 安全、协议正确性 | `projects/*/` |

### 批次 3 — 文档与卫生

| ID | 维度 | 检查内容 | 范围 |
|----|------|----------|------|
| D1 | **文档漂移 + 仓库卫生** | docs/ vs 实际代码交叉验证、CLAUDE.md 准确性、_eval/ 清理、.gitignore、孤儿文件、TODO/FIXME | `docs/` + `CLAUDE.md` + `_eval/` + `templates/` + `wheels/WISHLIST.md` |
| H1 | **HAL port** | 寄存器 RMW、busy-wait、中断安全、时钟配置 | `vsf-addon/` |
| S1 | **安全 + 测试覆盖** | 密钥泄露、硬编码凭据 + 公开函数 vs 测试覆盖盲区 | 全仓库（git ls-files） |
| B1 | **构建系统 + 依赖图** | CMakeLists 标志一致性 + ADR-009 方向/循环/文档化 | 全部 `test/CMakeLists.txt` + wheel 依赖 |

### 批次 4 — 深度质量

| ID | 维度 | 检查内容 | 范围 |
|----|------|----------|------|
| Q1 | **可移植性** | GCC 扩展（`__attribute__`、`typeof`、嵌套函数）、endianness 假设（raw byte 组装是否用移位而非 cast）、`sizeof(int)` 依赖、对齐假设（`__packed`）、非 C99 特性 | 全部 wheel .h/.c |
| Q2 | **License 合规** | vendor/第三方代码归属完整性、开源协议兼容性（MIT/Apache-2.0/GPL 冲突）、VENDOR.md 中 upstream URL/版本/license 是否完整 | `wheels/*/vendor/` + `_eval/` + `vsf/` license |
| Q3 | **内存安全** | 悬垂指针、use-after-free 模式、未初始化读取、double-free、函数返回栈变量地址 | 全部 wheel .c |
| Q4 | **错误码一致性** | 所有 wheel 的错误返回模式（enum vs int vs bool）、错误码命名（`_ERR_` vs `_FAIL_`）、NULL 入参返回值是否统一 | 全部 wheel .h |
| Q5 | **命名深度扫描** | struct 字段名模式（`ctx` vs `user_data`）、enum 值前缀、宏大小写、typedef 后缀（`_t` vs `_fn`）、跨 wheel 同类概念命名一致性 | 全部 wheel .h |
| Q6 | **README 示例可编译** | 每个 wheel README 中的代码片段提取后能否通过 C99 编译（语法正确、函数签名匹配当前 API） | 全部 wheel README.md |

### 批次 5 — 静态分析级

| ID | 维度 | 检查内容 | 范围 |
|----|------|----------|------|
| Q7 | **MISRA-C 子集合规** | Rule 10.x 隐式转换、Rule 14.x 控制流（unreachable code）、Rule 17.x 函数（隐式声明、参数不匹配）、Rule 21.x 禁用标准库函数（atoi/atof/sprintf→snprintf） | 全部 wheel .c |
| Q8 | **防御性编程完整性** | switch 必须有 default、if-else 链兜底、除法前检查除数非零、移位量 < 位宽、循环有上界 | 全部 wheel .c |
| Q9 | **数值精度** | float == 比较、累积浮点误差、整数截断（uint32→uint16 无 guard）、有符号/无符号混用比较 | 全部 wheel .c |
| Q10 | **头文件自包含** | 每个 .h 是否包含自身需要的全部 #include（不依赖 .c 的 include 顺序） | 全部 wheel .h |
| Q11 | **宏卫生** | 多次求值风险（宏参数含副作用）、缺少外层括号、命名空间污染（过于通用的宏名如 MAX/MIN/OK） | 全部 wheel .h/.c |
| Q12 | **测试质量** | 是否只验证 happy path、是否缺少边界值（0/MAX/溢出）、assert 密度、空测试体、error path 覆盖 | 全部 wheel test/*.c |

### 批次 6 — 语义级 / 架构级

| ID | 维度 | 检查内容 | 范围 |
|----|------|----------|------|
| Q13 | **死代码检测** | .c 中定义但全仓库无引用的 static 函数、未走到的 else/case 分支 | 全部 wheel .c |
| Q14 | **圈复杂度** | 每个函数 cyclomatic complexity，标记 >15 的函数 | 全部 wheel .c |
| Q15 | **测试隔离性** | 测试间共享全局变量是否在每个 TEST 前重置、测试顺序依赖 | 全部 wheel test/*.c |
| Q16 | **编译器警告实扫** | 用 `-Wconversion -Wsign-conversion` 实际编译收集真实警告 | 全部 wheel test/CMakeLists.txt |
| Q17 | **API 表面积** | 每个 wheel 公开函数数 vs 代码行数、标记 >15 函数或 ratio 异常的 wheel | 全部 wheel .h |
| Q18 | **栈深度分析** | 每个公开函数最深调用链层数 + 栈上局部变量字节数 | 全部 wheel .c |

### 批次 7 — 领域级 / 工程级

| ID | 维度 | 检查内容 | 范围 |
|----|------|----------|------|
| Q19 | **Datasheet 交叉验证** | L1 driver 寄存器地址、位域、默认值 vs 芯片 datasheet 一致性 | `wheels/drivers/` 全部 .c |
| Q20 | **物理单位一致性** | 输出单位（G/deg·s⁻¹/Pa/lux/℃）是否在 header + README 标注，跨 wheel 统一 | 全部 wheel .h + README |
| Q21 | **ARM 二进制体积** | arm-none-eabi-gcc 交叉编译每个 wheel，报告 .text/.data/.bss 大小 | 全部 wheel .c |
| Q22 | **配置宏文档化** | `#ifndef` 可调宏是否在 README Configuration 表中列出 | 全部 wheel .h/.c + README |
| Q23 | **抽象泄漏** | header 是否暴露内部 struct 字段 / 内部宏，应 opaque 的是否暴露 | 全部 wheel .h |
| Q24 | **测试变异韧性** | 手动翻转关键条件（< → >=），现有测试能否捕获 | 代表性 15 wheel .c + test |

## 每维度输出规范

每个维度审查完成后，AI 按以下格式汇报：

```
### <维度 ID>: <维度名>

**CRITICAL** (N 项)
| # | 文件:行 | 问题 | 修复方式 |

**MODERATE** (N 项)
| # | 文件:行 | 问题 | 修复方式 |

**MINOR** (N 项)
| # | 文件:行 | 问题 |

**已知不改** (N 项，附理由)
```

## 超级审计执行计划

```
1. 启动批次 1（W1 + W2 + W3）— 3 个 agent 并行
2. 启动批次 2（T1 + C1 + P1）— 3 个 agent 并行
3. 启动批次 3（D1 + H1 + S1 + B1）— 4 个 agent 并行
4. 启动批次 4（Q1 + Q2 + Q3, Q4 + Q5 + Q6）— 2-3 个 agent 并行
5. 启动批次 5（Q7 + Q8 + Q9, Q10 + Q11 + Q12）— 2-3 个 agent 并行
6. 启动批次 6（Q13 + Q14 + Q15, Q16 + Q17 + Q18）— 2-3 个 agent 并行
7. 启动批次 7（Q19 + Q20 + Q21, Q22 + Q23 + Q24）— 2-3 个 agent 并行
8. 每个 agent 返回后立即开始修复
7. 全部修复完成后分类 commit（按 type × scope 拆分）
8. 跑 test_all.sh + audit_wheels.py 验证
9. 推送
```

完整超级审计（36 维度）从启动到推送约 60-80 分钟。

## 审计频率建议

| 场景 | 推荐 |
|------|------|
| 每完成一批 wheel（10+） | 超级审计 |
| 新项目整合完成 | P1 + D1 |
| CI 改动后 | C1 |
| vsf-addon 改动后 | H1 |
| 发布前 | 超级审计 |
| 日常开发 | 按需单维度 |
| 季度例行 | Q1-Q6 深度质量批次 |
