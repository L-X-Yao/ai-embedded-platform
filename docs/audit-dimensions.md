# 平台审计维度清单

本文档定义了 AI 可执行的全部审计维度。用于任务类型 11（平台自省）的标准化执行。

## 交互协议

| 指令 | 行为 |
|------|------|
| `审计` | 列出维度表，等用户选择 |
| `审计 <维度ID>` | 单维度审查 + 修复 |
| `超级审计` | 三阶段执行（自动 → 人工 → 记录），见执行计划 |

## 40 个审计维度

自动化状态：
- 🤖 = audit_wheels.py 或脚本已完全覆盖，超级审计跳过
- ⚡ = 有自动化工具辅助，agent 只查工具未覆盖的部分
- 👁️ = 纯人工/AI 审阅，无自动化工具

### 批次 1 — Wheel 源码

| ID | 维度 | 自动化 | 检查内容 | 范围 |
|----|------|:------:|----------|------|
| W1 | **Wheel 代码 (L0)** | 👁️ | 逻辑错误、溢出、UB、API 一致性、线程安全、边界测试缺失 | `wheels/primitives/` 全部 .h/.c/test |
| W2 | **Wheel 代码 (L1)** | 👁️ | 同上 + 寄存器/位掩码 vs datasheet、HAL 注入正确性 | `wheels/drivers/` 全部 |
| W3 | **Wheel 代码 (L2)** | 👁️ | 同上 + 协议合规性、恶意输入安全性 | `wheels/protocols/` 全部 |

**W1 拆分规则**：L0 有 30 个 wheel，单 agent 放不下。按字母拆分：
- W1a: bitfield → kvs（前 15）
- W1b: led_pattern → timeout（后 15）

### 批次 2 — 基础设施

| ID | 维度 | 自动化 | 检查内容 | 范围 |
|----|------|:------:|----------|------|
| T1 | **工具链脚本** | 👁️ | shell 注入、未引号变量、逻辑错误、可移植性 | `tooling/scripts/` + `.githooks/` |
| C1 | **CI + 跨 wheel 一致性** | ⚡ | CI job 逻辑、缓存、失败模式（规范检查 8 项已自动化，agent 只查 CI 逻辑） | `.github/workflows/` |
| P1 | **项目代码** | 👁️ | 竞态、栈溢出、Flash 安全、协议正确性。**前置：flybox 需 HIL 硬件** | `projects/*/` |

### 批次 3 — 文档与卫生

| ID | 维度 | 自动化 | 检查内容 | 范围 |
|----|------|:------:|----------|------|
| D1 | **文档漂移 + 仓库卫生** | ⚡ | docs/ vs 实际代码交叉验证、CLAUDE.md 准确性（WISHLIST staleness 已自动化） | `docs/` + `CLAUDE.md` + `_eval/` + `templates/` |
| H1 | **HAL port** | 👁️ | 寄存器 RMW、busy-wait、中断安全、时钟配置 | `vsf-addon/` |
| S1 | **安全 + 测试覆盖** | ⚡ | 密钥泄露、硬编码凭据（pre-commit 已拦截 .env/.pem）+ 公开函数 vs 测试覆盖盲区 | 全仓库 |
| B1 | **构建系统 + 依赖图** | ⚡ | CMakeLists 标志一致性（C99/-Werror 已自动化）+ ADR-009 方向/循环 | 全部 `test/CMakeLists.txt` |

### 批次 4 — 深度质量

| ID | 维度 | 自动化 | 检查内容 | 范围 |
|----|------|:------:|----------|------|
| Q1 | **可移植性** | 👁️ | GCC 扩展、endianness 假设、`sizeof(int)` 依赖、对齐假设、非 C99 特性 | 全部 wheel .h/.c |
| Q2 | **License 合规** | 👁️ | vendor 归属完整性、协议兼容性、VENDOR.md 完整性 | `wheels/*/vendor/` + `_eval/` |
| Q3 | **内存安全** | 👁️ | 悬垂指针、use-after-free、未初始化读取、double-free、返回栈地址 | 全部 wheel .c |
| Q4 | **错误码一致性** | 🤖 | `audit_wheels.py` 的 `has_negative_error_codes()` 已完全覆盖 | — |
| Q5 | **命名深度扫描** | 👁️ | struct 字段名、enum 前缀、宏大小写、typedef 后缀、跨 wheel 一致性 | 全部 wheel .h |
| Q6 | **README 示例可编译** | 👁️ | 代码片段提取后能否通过 C99 编译 | 全部 wheel README.md |

### 批次 5 — 静态分析级

| ID | 维度 | 自动化 | 检查内容 | 范围 |
|----|------|:------:|----------|------|
| Q7 | **MISRA-C 子集合规** | 👁️ | Rule 10.x 隐式转换、14.x 控制流、17.x 函数、21.x 禁用函数 | 全部 wheel .c |
| Q8 | **防御性编程完整性** | 👁️ | switch default、除法检零、移位 < 位宽、循环有上界 | 全部 wheel .c |
| Q9 | **数值精度** | 👁️ | float == 比较、累积误差、整数截断、有符号/无符号混用 | 全部 wheel .c |
| Q10 | **头文件自包含** | 👁️ | 每个 .h 是否包含自身需要的全部 #include | 全部 wheel .h |
| Q11 | **宏卫生** | 👁️ | 多次求值风险、缺少外层括号、命名空间污染 | 全部 wheel .h/.c |
| Q12 | **测试质量** | ⚡ | happy path only、边界缺失、assert 密度（TEST/RUN 平衡已自动化） | 全部 wheel test/*.c |

### 批次 6 — 语义级 / 架构级

| ID | 维度 | 自动化 | 检查内容 | 范围 |
|----|------|:------:|----------|------|
| Q13 | **死代码检测** | 👁️ | 无引用的 static 函数、未走到的 else/case | 全部 wheel .c |
| Q14 | **圈复杂度** | 👁️ | 每函数 cyclomatic complexity，标记 >15 的 | 全部 wheel .c |
| Q15 | **测试隔离性** | 👁️ | 共享全局变量在每个 TEST 前是否重置 | 全部 wheel test/*.c |
| Q16 | **编译器警告实扫** | ⚡ | `-Wconversion -Wsign-conversion` 实际编译。ARM 交叉编译已有 `arm_size_audit.sh` | 全部 wheel |
| Q17 | **API 表面积** | 👁️ | 公开函数数 vs 代码行数，标记 >15 函数 | 全部 wheel .h |
| Q18 | **栈深度分析** | 👁️ | 最深调用链层数 + 栈上局部变量字节数 | 全部 wheel .c |

### 批次 7 — 领域级 / 工程级

| ID | 维度 | 自动化 | 检查内容 | 范围 |
|----|------|:------:|----------|------|
| Q19 | **Datasheet 交叉验证** | 👁️ | 寄存器地址、位域、默认值 vs 芯片 datasheet | `wheels/drivers/` 全部 .c |
| Q20 | **物理单位一致性** | 👁️ | 输出单位是否在 header + README 标注，跨 wheel 统一 | 全部 wheel .h + README |
| Q21 | **ARM 二进制体积** | 🤖 | `arm_size_audit.sh` 已完全覆盖。结果见 `docs/arm-binary-sizes.md` | — |
| Q22 | **配置宏文档化** | 👁️ | `#ifndef` 可调宏是否在 README Configuration 表中列出 | 全部 wheel .h/.c + README |
| Q23 | **抽象泄漏** | 👁️ | header 暴露内部 struct 字段 / 内部宏 | 全部 wheel .h |
| Q24 | **测试变异韧性** | 👁️ | 翻转关键条件（< → >=），测试能否捕获 | 全部 >5 公开函数的 wheel |

### 批次 8 — 精细化

| ID | 维度 | 自动化 | 检查内容 | 范围 |
|----|------|:------:|----------|------|
| Q25 | **Const 正确性** | 👁️ | 只读指针参数是否标 const | 全部 wheel .h |
| Q26 | **代码克隆检测** | 👁️ | 跨 wheel 重复代码块（>10 行相似） | 全部 wheel .c |
| Q27 | **@thread_safe 标注准确性** | ⚡ | 标注存在性已自动化，语义准确性需人工 | 全部 wheel .h + .c |
| Q28 | **错误路径资源泄漏** | 👁️ | early return 跳过清理 | 全部 wheel .c |
| Q29 | **函数级文档完整性** | 🤖 | `audit_wheels.py` 的 `has_function_doc_all()` 已完全覆盖 | — |
| Q30 | **枚举类型安全** | 👁️ | enum 作数组下标越界检查、隐式 enum→int | 全部 wheel .c |

## 每维度输出规范

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

### Phase 0 — 自动化 gate（2 分钟）

```bash
make check                    # test_all.sh + audit_wheels.py --quiet
wsl bash tooling/scripts/arm_size_audit.sh   # Q21
```

如果 FAIL → 先修再继续。Phase 0 覆盖：Q4, Q21, Q29, 以及 C1/B1/D1/S1/Q12/Q27 的自动化子集。

### Phase 1 — AI 审阅（按需并行）

跳过 🤖 维度（Q4, Q21, Q29）。对 ⚡ 维度，agent prompt 指明已自动化的子检查。

```
轮次 1: W1a + W1b + W2 + W3           — 4 agent（代码审阅）
轮次 2: T1 + C1(仅CI逻辑) + P1(如有硬件) — 2-3 agent
轮次 3: D1 + H1 + S1(仅覆盖盲区) + B1(仅依赖图) — 3-4 agent
轮次 4: Q1+Q2+Q3, Q5+Q6               — 2 agent
轮次 5: Q7+Q8+Q9, Q10+Q11+Q12(仅测试质量) — 2 agent
轮次 6: Q13+Q14+Q15, Q16+Q17+Q18      — 2 agent
轮次 7: Q19+Q20, Q22+Q23+Q24          — 2 agent
轮次 8: Q25+Q26+Q27(仅语义), Q28+Q30  — 2 agent
```

每个 agent 返回后：
1. grep 验证关键发现（假阳性率 ~20-30%）
2. 修复确认的问题
3. 立即跑 `test_all.sh`

### Phase 2 — 记录 + 推送（5 分钟）

```
1. 按 type × scope 拆 commit
2. 跑 make check 最终验证
3. git push（pre-push hook 自动跑 audit）
4. 更新本文档末尾的"审计历史"表
```

完整超级审计（40 维度）从启动到推送约 60-90 分钟。

## 执行纪律

1. **🤖 维度不启动 agent** — 自动化已覆盖，浪费上下文
2. **agent 返回后先 grep 验证关键发现再修** — 假阳性率约 20-30%
3. **每批修复后立即 `test_all.sh`** — 不攒到最后
4. **存活变异单独追踪** — 修完基线后逐个补杀
5. **P1 需 HIL 硬件** — 无硬件时跳过或仅做代码分析

## 扩展协议

超级审计完成后可选两类扩展，不是新维度：

### 参数扩展（同维度换规则集）

命名规范：`<维度ID>+`，如 `Q7+`、`Q16+`、`Q24+`。

| 扩展 | 内容 |
|------|------|
| Q7+ | MISRA Rule 11（指针转换）/ Rule 15（goto）/ Rule 16（switch 完整性） |
| Q16+ | 编译加 `-Wcast-align -Wdouble-promotion -Wformat=2`（ARM 交叉编译） |
| Q24+ | 对首轮未覆盖的 wheel 补充变异测试 |

### 交叉分析（维度组合）

命名规范：`<维度A>×<维度B>`，如 `Q14×Q24`。

| 交叉 | 内容 |
|------|------|
| Q14×Q24 | 高复杂度函数的变异韧性（每函数 3 个变异点深度分析） |
| Q23×Q29 | LEAKING wheel 的内部字段是否有文档标注 |
| Q18×Q14 | 高栈 + 高复杂度函数的链式调用风险矩阵 |

## 审计频率建议

| 场景 | 推荐 |
|------|------|
| 每次 push | Phase 0 自动执行（pre-push hook + CI） |
| 每完成一批 wheel（10+） | 超级审计 |
| 新项目整合完成 | P1 + D1 |
| CI 改动后 | C1 |
| vsf-addon 改动后 | H1 |
| 发布前 | 超级审计 |
| 日常开发 | 按需单维度 |

## 审计历史

| 日期 | 范围 | 发现 | 修复 | 备注 |
|------|------|------|------|------|
| 2026-04-30 | 超级审计（40 维度首轮） | 110+ | 110+ | 首次全量，含 flybox 11 项待 HIL |
| 2026-04-30 | 循环审计（audit_wheels.py） | 20 | 20 | Q4 error enum + Q29 doc comments |
| 2026-04-30 | Q21 ARM binary size | 2 | 2 | max7219 stddef.h + log type-limits |
