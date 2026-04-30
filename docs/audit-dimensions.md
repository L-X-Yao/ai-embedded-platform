# 平台审计维度清单

本文档定义了 AI 可执行的全部审计维度。用于任务类型 11（平台自省）的标准化执行。

## 交互协议

| 指令 | 行为 |
|------|------|
| `审计` | 列出维度表，等用户选择 |
| `审计 <维度名>` | 单维度审查 + 修复 |
| `超级审计` | 全维度并行启动（7 个并行批次），按完成顺序汇报 + 修复 |

## 12 个审计维度

### 批次 1（并行启动）

| ID | 维度 | 检查内容 | 范围 |
|----|------|----------|------|
| W1 | **Wheel 代码 (L0)** | 逻辑错误、溢出、UB、API 一致性、线程安全、边界测试缺失 | `wheels/primitives/` 全部 .h/.c/test |
| W2 | **Wheel 代码 (L1)** | 同上 + 寄存器/位掩码 vs datasheet、HAL 注入正确性 | `wheels/drivers/` 全部 |
| W3 | **Wheel 代码 (L2)** | 同上 + 协议合规性、恶意输入安全性 | `wheels/protocols/` 全部 |

### 批次 2（并行启动）

| ID | 维度 | 检查内容 | 范围 |
|----|------|----------|------|
| T1 | **工具链脚本** | shell 注入、未引号变量、逻辑错误、可移植性 | `tooling/scripts/` + `.githooks/` |
| C1 | **CI + 跨 wheel 一致性** | job 逻辑、缓存、失败模式 + extern C/thread_safe/harness/C99/-Werror/TEST-RUN/VSF-native/include-guard/API-review 8 项规范检查 | `.github/workflows/` + 全部 wheel |
| P1 | **项目代码** | 竞态、栈溢出、Flash 安全、协议正确性 | `projects/*/` |

### 批次 3（并行启动）

| ID | 维度 | 检查内容 | 范围 |
|----|------|----------|------|
| D1 | **文档漂移 + 仓库卫生** | docs/ vs 实际代码交叉验证、CLAUDE.md 准确性、_eval/ 清理、.gitignore、孤儿文件、TODO/FIXME | `docs/` + `CLAUDE.md` + `_eval/` + `templates/` + `wheels/WISHLIST.md` |
| H1 | **HAL port** | 寄存器 RMW、busy-wait、中断安全、时钟配置 | `vsf-addon/` |
| S1 | **安全 + 测试覆盖** | 密钥泄露、硬编码凭据 + 公开函数 vs 测试覆盖盲区 | 全仓库（git ls-files） |
| B1 | **构建系统 + 依赖图** | CMakeLists 标志一致性 + ADR-009 方向/循环/文档化 | 全部 `test/CMakeLists.txt` + wheel 依赖 |

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
3. 启动批次 3（D1 + H1 + S1 + B1）— 同时或按返回顺序
4. 每个 agent 返回后立即开始修复
5. 全部修复完成后分类 commit（按 type × scope 拆分）
6. 跑 test_all.sh + audit_wheels.py 验证
7. 推送
```

合并后实际为 **7 个并行批次**（受 agent 并发限制，分 2-3 轮启动）。
完整超级审计从启动到推送约 20-30 分钟。

## 审计频率建议

| 场景 | 推荐 |
|------|------|
| 每完成一批 wheel（10+） | 超级审计 |
| 新项目整合完成 | P1 + D1 |
| CI 改动后 | C1 |
| vsf-addon 改动后 | H1 |
| 发布前 | 超级审计 |
| 日常开发 | 按需单维度 |
