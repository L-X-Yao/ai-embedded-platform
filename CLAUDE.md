# ai-embedded-platform

AI 驱动的嵌入式开发母仓库。**不是**学习仓库、不是 sandbox，是公司真实嵌入式项目的底座。

## 核心愿景

- 嵌入式软硬件团队（2-3 人）+ AI 协作做项目
- 目标分工：**人 10%**（需求 / 关键技术决策 / 架构 / 终审），**AI 90%**（实现 / 测试 / 文档 / 配置）
- 随项目积累，`wheels/` + `templates/` 越来越厚，新项目起点逐步降为"配参数 + 写应用"
- 时间尺度：前 2-3 个项目是投资期（AI 占比 40-50%），第 4 个起 ROI 显著正，24 月稳态 80%+

## 给 AI 的工作原则（按重要度）

### 0. 零号铁律：新建 wheel 前先 grep VSF 原生

完整策略见 **[ADR-006](docs/adr/006-prefer-vsf-native-over-custom-wheel.md)**。摘要：

```bash
grep -rn "功能关键词" vsf/source/service/
grep -rn "功能关键词" vsf/source/utilities/
grep -rn "功能关键词" vsf/source/component/
```

速查表见 `docs/debugging-playbook.md`。新 wheel 的 README 必须有 "Why not VSF native?" 节。**违反就 ring_buffer 教训重演**。

### 1. 绝不从零发挥

进入任何任务：
1. `ls wheels/` + `ls wheels/_draft/`
2. `ls templates/`
3. grep VSF 原生（见上）

找到就用，不要自造。

### 2. 读 canonical example 再动手

尤其 VSF 相关：文档稀疏、有静默失败。写代码前**通读一个工作的示例**。参考 `memory/feedback_niche_codebase_read_first.md`。

### 3. 所有产出可验证

- 自动化测试（pytest + pexpect 或 subprocess）
- host sim 里能跑（VSF linux64 target / QEMU）
- 测试不绿不叫完成

HIL 当前是人工（见 ADR-002）——AI 生成粘贴模板 → 用户手烧录 → 日志粘回。

### 4. 踩坑即记录

更新：
- `docs/debugging-playbook.md`（公共）
- wheel 的 `README.md`（局部）
- `memory/`（跨项目规律）

### 5. 配置优先于代码

能用 CMake 变量 / `.config` / 宏控制的，不要写代码分支。

### 6. 保守边界

**不要自作主张**：
- 改 `vsf/` 上游代码（必须走 patch + `tooling/patches/` 流程，见 ADR-005）
- 动 `projects/` 真实项目架构（只在明确任务范围内改）
- 删代码 / 历史文件（先归档 `_eval/` 或问用户）
- 沉淀阶段擅自决定 wheel API（必须等用户 15 分钟审批，见 `ai-workflow.md` 任务类型 9）

## 目录结构

```
ai-embedded-platform/
├── CLAUDE.md                       本文件
├── docs/
│   ├── philosophy.md               10/90 愿景
│   ├── wheel-contract.md           轮子合格定义
│   ├── api-conventions.md          跨 wheel 命名/错误/内存/线程约定
│   ├── ai-workflow.md              10 类任务标准路径
│   ├── debugging-playbook.md       踩坑指南 + VSF 数据结构速查
│   └── adr/                        7 个架构决策记录
├── vsf/                            上游源码（干净，不直接改）
├── vsf-addon/                      新增代码（新 MCU port / 新组件）
├── wheels/
│   ├── primitives/                 L0：工具原语（纯软件）
│   ├── drivers/                    L1：硬件驱动（按芯片型号）
│   ├── protocols/                  L2：协议 / 服务
│   ├── modules/                    L3：功能模块
│   └── _draft/                     孵化区
├── templates/                      项目起步模板
├── projects/                       真实项目
├── tooling/
│   ├── patches/                    VSF 上游修改（patch 文件）
│   ├── hil/                        硬件在环自动化（未来）
│   └── scripts/                    通用工具脚本
├── memory/                         AI 跨对话持久记忆
└── _eval/                          评估历史遗留物
```

## Wheel 4 层

| 层 | 目录 | 举例 | 验证 |
|----|------|------|------|
| L0 原语 | `primitives/` | log / cli_parser / state_machine / crc | sim 100% + ≥80% 覆盖 |
| L1 驱动 | `drivers/` | mpu6050 / w25q64 / lcd_st7789 | sim mock + ≥1 真机 |
| L2 协议 | `protocols/` | mqtt_client / ble_uart_service / at_command | sim loopback + 一致性 |
| L3 模块 | `modules/` | ota_updater / data_logger / cli_shell | 集成 sim + 真机 |

详见 `docs/wheel-contract.md`。

## 任务类型 → 做法

见 `docs/ai-workflow.md`（10 类）：

| 类型 | 特别注意 |
|------|---------|
| 1. 新建 wheel | grep VSF → API 审批 → 草稿 → 测试 → 升级 |
| 2. 新建项目 | 选 template → 配硬件 → 写项目 CLAUDE.md |
| 3. 加功能 | 先查 wheels，不偷偷在项目里重造 |
| 4. 修 bug | 复现成 failing test → 查 playbook → 修 → 更新 playbook |
| 5. 技术选型 | 2-4 候选 + 对比表 + 推荐 → 人拍板 → 记 ADR |
| 6. 移植（换 MCU） | 走 `vsf-addon/hal/driver/<vendor>/` |
| 7. 客户代码抽取 | 按分类过滤 → 剥离 IP → API 审批 → 抽 wheel |
| 8. VSF 上游修改 | patch 文件走 `tooling/patches/`，保持 vsf/ 干净 |
| 9. **沉淀**（项目结束后）🔑 | AI 扫 + API 提案 → 人 15 分钟审批 → AI 实现 |
| 10. 代码审查 | 对照 wheel-contract + api-conventions + 项目 CLAUDE |

## 平台选择：VSF

主基座：**VSF**（Versaloon Software Framework）——永久基座（ADR-007）。
- 强项：USB 主从、BLE、GUI、小资源 MCU、统一 HAL、皮肤机制
- Upstream 死掉 AI 代维护，不为切换 RTOS 做架构预案
- 客户强指定其他 RTOS → 通过 VSF 皮肤层适配

详见 `docs/adr/001-platform-choice-vsf.md` 和 `docs/adr/007-vsf-as-permanent-base.md`。

## 环境

- OS：Windows 11 + WSL2 Ubuntu 24.04
- 工具链（WSL 里）：gcc 13 / cmake / ninja / pytest / pexpect
- VSF 路径（WSL）：`/mnt/d/temp/project/ai-embedded-platform/vsf`
- PLOOC 子模块：已 clone 在 `vsf/source/utilities/3rd-party/PLOOC/raw/`

## 关键决策（ADR）

| # | 决策 |
|---|------|
| 001 | VSF 为基座 |
| 002 | Wheel sim 优先验证 |
| 003 | 客户代码分类回流 + IP 剥离 |
| 004 | Monorepo，wheels 外置于 vsf/ |
| 005 | VSF 修改走 patches + vsf-addon |
| 006 | 新 wheel 前必须 grep VSF 原生 |
| 007 | VSF 永久基座，不为切 RTOS 做预案 |

全部见 `docs/adr/README.md`。

## Commit 规范（严格）

**格式**（不可破）：

```
<type>(<scope>): <summary>

<optional body>

<optional footer>
```

### type（封闭集）

只允许已列出的类型。**遇到不在列表里的场景，AI 必须先问用户拍板并把新类型追加到本节，再 commit。**

- `docs` — 文档改动（`docs/`、`CLAUDE.md`、`README.md`、各 wheel README）
- `wheel` — wheel 代码或结构改动（不含其 README，那归 `docs`）

### scope（可选，与目录对齐）

- `wheels/_draft/<name>/` → `_draft/<name>`
- `wheels/<层>/<name>/` → `<层>/<name>`（例：`primitives/log`）
- `docs/adr/` → `adr`
- 顶层或跨多个 scope（`CLAUDE.md`、跨目录改动）→ 省略

小写、kebab-case。

### summary

- 小写、**祈使语气动词开头**（`add` / `fix` / `trim` / `tighten` / `codify` / `relax` / `freeze`…）
- **无结尾句号**
- 整行 < 70 字符
- 英文；禁止中英混排
- 标点严格：`<type>`、`(<scope>)`、`:` 之间不空格；`:` 后**一个**空格

### body（可选）

- 说 **why**（原因 / 触发 / 权衡）——diff 已显示 what
- 空行与标题隔开；每行 ≤ 72 字符
- 单纯格式化 / typo / rename 可省 body

### footer（可选）

- 引用 ADR：`Relates to: ADR-NNN`
- 引用既有 commit：`Follows <short-sha>`
- AI 生成 commit **必带**（精确一行，模型变就换对应行）：
  ```
  Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
  ```

### 拆分

- 一 commit = 一逻辑改动
- 跨 type 或跨 scope 必拆

### AI 易踩的坑

- ❌ Conventional Commits 类型（`feat:` / `chore:` / `refactor:` / `build:` / `ci:`）
- ❌ summary 中英混排
- ❌ summary 结尾加句号
- ❌ summary 名词短语无动词（反例 `first wheel — leveled logging`；正例 `add leveled logging wheel`）
- ❌ body 只说 what（反例 `update log.c`；正例 `fix race in log_emit flush`）

### 新场景处理（escape hatch）

遇到本规范未覆盖的情况（新 type / 新 scope 结构 / 特殊 footer / ...），AI **必须先问用户拍板**，拍板后将新规则追加到本节，再 commit。**不得沉默创造**。

## 当前状态

Sprint 0 + Sprint 0.1（自审改进）已完成：
- ✅ 目录结构（docs/adr / wheels L0-L3 / vsf-addon / tooling/patches）
- ✅ 核心文档（philosophy / wheel-contract / api-conventions / ai-workflow / debugging-playbook）
- ✅ 7 个 ADR（平台决策历史）
- ✅ tooling/patches + apply.sh 骨架
- ✅ vsf-addon 骨架
- ✅ memory 7 条（用户背景 + 战略决策 + 4 条 feedback）
- ✅ 历史评估归档 `_eval/`
- ✅ git 仓库（8 commits，vsf/ vendored 不 fork）
- ✅ **第一个 wheel：`wheels/_draft/log/`** —— 带等级 + 模块 + 编译期过滤 + 多 sink；11 C 测试 + 4 pytest（含 release 构建编译期消除验证）全绿；作为后续 wheel 的 canonical 例
- ✅ `wheels/WISHLIST.md`（记忆库，不是 TODO）

未开展：
- 第一个真实项目（待用户提 PoC——触发 `ai-workflow.md` 任务类型 2）
- 基础 wheels（按项目触发，不预制。WISHLIST 是记忆而非承诺）
- HIL 自动化（当前人工粘日志过渡）
- `tooling/scripts/test_downstream.sh`（等 projects/ 有内容再写）
- 硬件平台 port（国民技术/兆易/先辑——项目来时走 `vsf-addon/hal/driver/<vendor>/`）

## 下一步触发点（Sprint 0 已封版）

**无真实项目触发前不改 `docs/`**。继续 polish 文档 ROI 为负（规则未经项目检验 + 文档-代码比例已严重倒挂：~2000 行文档 vs 1 个 draft wheel）。

解冻条件（触发就开工，未触发就别动文档）：

- 用户说"开始第 N 个项目 yyy" → 走 `ai-workflow.md` 任务类型 2
- 用户说"加个 wheel xxx" → 走任务类型 1，**强制先 grep VSF**
- 用户说"项目 zzz 交付了" → 走任务类型 9（沉淀）
- 用户主动要求"平台自省" → 走任务类型 11（必须基于新发现，不做无素材 polish）
