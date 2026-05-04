# ai-embedded-platform

AI 驱动的嵌入式开发母仓库。**不是**学习仓库、不是 sandbox，是公司真实嵌入式项目的底座。

## 核心愿景

- 嵌入式软硬件团队（2-3 人）+ AI 协作做项目
- 目标分工：**人 10%**（需求 / 关键技术决策 / 架构 / 终审），**AI 90%**（实现 / 测试 / 文档 / 配置）
- 随项目积累，`wheels/` + `templates/` 越来越厚，新项目起点逐步降为"配参数 + 写应用"
- 时间尺度：前 2-3 个项目是投资期（AI 占比 40-50%），第 4 个起 ROI 显著正，24 月稳态 80%+

## 新项目启动清单（Day 1）

1. `mkdir -p projects/<name>` + `cp templates/project-integration/CLAUDE.md.template projects/<name>/CLAUDE.md`
2. 填写项目 CLAUDE.md（硬件表 / 技术栈 / VSF 迁移决策）
3. `ls wheels/` 查可用 wheel，选需要的列入项目 CLAUDE.md
4. 候选 wheel 同步到 `wheels/WISHLIST.md`（信号计数 +1）
5. 搭建构建系统，接入 wheel（见 [`docs/integrating-wheels.md`](docs/integrating-wheels.md)）
6. 加 CI job（参考 `.github/workflows/ci.yml` flybox-build）
7. 首次构建通过 → 开始写功能

## 给 AI 的工作原则（按重要度）

### -1. Session 生命周期

**开始**（新 session 或上下文压缩后）：
```bash
make bootstrap          # git 状态 / wheel 表 / audit / 最近踩坑
```
一条命令替代 5-8 个 tool call。压缩后用 `--short` 快速刷新。

**结束**（commit 完成后、切换任务前、用户说"到此为止"时）：
1. `make check` — 确保测试 + 审计干净
2. 如有 worktree 分支未合并 → `dispatch_agent.sh --merge-all`
3. `git push` — 推送所有改动
4. 写 memory — 本次踩坑、决策、用户偏好变化
5. 如有 CLAUDE.md 变更（wheel 数量、工具数量等）→ 更新并 commit
6. 如有大量工作 → 一句话告知用户下次 session 从哪里继续

**交接**（换 session / 换窗口 / context 即将满）：
- 关键上下文写入 memory（不是 CLAUDE.md——memory 跨 session 持久）
- 未完成任务列为明确的 TODO 告知用户（不写进文件，口头说）
- 不要假设下一个 session 能看到当前对话

### 0. 核心原则：Wheel 是被动库

完整架构见 **[docs/architecture.md](docs/architecture.md)**。摘要：

- Wheel 不碰 OS（不创建任务、不加锁、不 sleep、不分配堆内存）
- 依赖只通过函数指针注入
- 所有 wheel 必须 host 可测

判断标准：如果一个功能需要"自己创建任务"或"自己管理中断"，它不是 wheel。

### 0.1 新建 wheel 前查通用开源实现

完整策略见 **[ADR-010](docs/adr/010-prefer-open-source-implementations.md)** + **[ADR-011](docs/adr/011-ecosystem-driven-wheel-building.md)**。

- **不再等项目触发**——按"嵌入式常用"标准主动构建（ADR-011）
- **查通用开源库**（jsmn / LittleFS / cJSON 等），能 vendor 就 vendor（ADR-010）
- **不查 VSF**——VSF 组件绑定 VSF 内核，与我们的零依赖 wheel 无关

### 1. 绝不从零发挥

进入任何任务：
1. `ls wheels/` + `ls wheels/_draft/`
2. `ls templates/`
3. 搜索通用开源实现（ADR-010）

找到就用，不要自造。

### 2. 读 canonical example 再动手

尤其 VSF 相关：文档稀疏、有静默失败。写代码前**通读一个工作的示例**。参考 `memory/feedback_niche_codebase_read_first.md`。

### 3. 所有产出可验证

- 自动化测试（pytest + pexpect 或 subprocess）
- host sim 里能跑（VSF linux64 target / QEMU）
- 测试不绿不叫完成

HIL 当前是人工（见 ADR-002）——AI 生成粘贴模板 → 用户手烧录 → 日志粘回。

### 4. 踩坑即记录 + 每轮工作后复盘

每完成一批工作（不是等用户问），主动回顾并写入 memory：
- **踩了什么坑？** → 记录，避免下次重复
- **什么方法特别快？** → 记录，下次直接复用
- **有新的架构/技术决策吗？** → 记录决策和理由
- **用户偏好/角色有变化吗？** → 更新 user 类型 memory

目的：**AI 越做越快，不重复犯错。** 每个 session 的经验沉淀到 memory，下个 session 站在这个基础上继续。

同步更新：
- `docs/debugging-playbook.md`（公共踩坑）
- wheel 的 `README.md`（局部）
- Claude Code 自动记忆（跨项目规律，由 Claude Code 管理，不在 repo 内）

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
│   ├── ai-workflow.md              12 类任务标准路径
│   ├── debugging-playbook.md       踩坑指南 + VSF 数据结构速查
│   └── adr/                        10 个架构决策记录
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
│   ├── audit/                      审计脚本
│   ├── test/                       test_harness.h + 测试脚本
│   ├── wheel/                      wheel 生命周期
│   ├── session/                    会话管理 + 派发
│   ├── metrics/                    平台度量
│   ├── ci/                         CI + 仓库维护
│   ├── fix/                        批量修复
│   ├── debug/                      MCU 调试工具包
│   ├── hil/                        硬件在环自动化
│   ├── patches/                    VSF 上游修改（patch 文件）
│   └── experiments/                VSF tryout scaffolds
└── _eval/                          评估历史遗留物
```

## Wheel 4 层

| 层 | 目录 | 举例 | 验证 |
|----|------|------|------|
| L0 原语 | `primitives/` | log / cli_parser / state_machine / crc | sim 100% + ≥80% 覆盖 |
| L1 驱动 | `drivers/` | mpu6050 / w25qxx / bmp280 | sim mock + ≥1 真机 |
| L2 协议 | `protocols/` | mqtt_packet / modbus_rtu / at_command | sim loopback + 一致性 |
| L3 模块 | `modules/` | ota_updater / data_logger / cli_shell | 集成 sim + 真机 |

详见 `docs/wheel-contract.md`。

## 任务类型 → 做法

见 `docs/ai-workflow.md`（12 类）：

| 类型 | 特别注意 |
|------|---------|
| 1. 新建 wheel | 查开源(ADR-010) → API 审批 → scaffold → 测试 → 升级 |
| 2. 新建项目 | 选 template → 配硬件 → 写项目 CLAUDE.md |
| 3. 加功能 | 先查 wheels，不偷偷在项目里重造 |
| 4. 修 bug | 复现成 failing test → 查 playbook → 修 → 更新 playbook |
| 5. 技术选型 | 2-4 候选 + 对比表 + 推荐 → 人拍板 → 记 ADR |
| 6. 移植（换 MCU） | 走 `vsf-addon/hal/driver/<vendor>/` |
| 7. 客户代码抽取 | 按分类过滤 → 剥离 IP → API 审批 → 抽 wheel |
| 8. VSF 上游修改 | patch 文件走 `tooling/patches/`，保持 vsf/ 干净 |
| 9. **沉淀**（项目结束后）🔑 | AI 扫 + API 提案 → 人 15 分钟审批 → AI 实现 |
| 10. 代码审查 | 对照 wheel-contract + api-conventions + 项目 CLAUDE |
| 11. 平台自省 | 40 维度审计（见 `docs/audit-dimensions.md`），支持 `审计` / `超级审计` 指令 |
| 12. 发布 | `release.sh` 打版本标签 + 生成 CHANGELOG |

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

## 多窗口并行开发

多个 Claude Code 窗口同时操作本仓库时，用 worktree 按 scope 隔离，避免文件冲突。

### 方式一：`claude -w`（推荐）

Claude Code 内置 worktree 管理，一行命令创建隔离分支，无改动时自动清理：

```powershell
# 终端 1：wheel 开发/审计
claude -w wheels -n "wheel-work"

# 终端 2：项目集成
claude -w project -n "project-work"

# 终端 3：主目录——tooling / docs / CI
claude
```

`-w <name>` 自动创建 `.claude/worktrees/<name>/` 目录和 `worktree-<name>` 分支。`-n` 命名 session 以便 `claude --resume` 恢复。

### 方式二：`dispatch_agent.sh` 自动派发（批量任务推荐）

用 `dispatch_agent.sh` 批量派发 `claude -p -w` agent，每个 agent 在独立 worktree 中工作并自动 commit：

```bash
# 派发（后台运行，立即返回 PID）
wsl bash tooling/session/dispatch_agent.sh \
  --name audit-w1 --budget 1.00 \
  --scope "wheels/primitives/" \
  --prompt "审计 W1: L0 全部 wheel"

# 查看所有 agent 状态
wsl bash tooling/session/dispatch_agent.sh --status

# 全部完成后一次性 squash merge + make check + push
wsl bash tooling/session/dispatch_agent.sh --merge-all --push
```

核心特性：
- `--name`：worktree 名（创建 `worktree-<name>` 分支）
- `--budget`：agent 最大花费（USD）
- `--prompt`：任务描述（脚本自动追加 commit 指令和 scope 限制）
- `--scope`：限制 agent 只能修改指定路径
- `--wait`：前台运行（阻塞直到完成）
- `--merge-all [--push]`：squash merge 所有 `worktree-*` 分支，自动 `make check`
- `--status`：显示各 worktree 分支的 commit 数和内容

适合超级审计、批量重构、跨 wheel 修复等需要多 agent 并行的场景。主窗口只做调度 + 合并，不做长线实现。

**铁律：所有 `claude -p -w` 派发必须通过 `dispatch_agent.sh`，禁止手写裸命令。** 脚本自动追加 commit 指令和 scope 限制，手写会漏导致 agent 不 commit、worktree 残留。

### 方式三：会话内 subagent

当前 session 内用 Agent tool + `isolation: "worktree"` 派出隔离 agent，不需要开新终端。适合委派单次任务（审计扫描、测试覆盖检查）。

### 方式四：Agent Teams（实验性）

多个 Claude 实例共享任务列表、互相通信。需先启用：

```json
{ "env": { "CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS": "1" } }
```

```powershell
claude -w audit --teammate-mode in-process
```

适合多角度并行审计（安全 + 性能 + 测试覆盖同时跑）。

### scope 分配（所有方式通用）

| 窗口 | 拥有的目录 | 可读但不改 |
|------|-----------|-----------|
| A（wheels） | `wheels/` | `tooling/test/`、`docs/api-conventions.md` |
| B（project） | `projects/<name>/`、`vsf-addon/` | `wheels/`（只用不改） |
| C（infra） | `tooling/`、`docs/`、`CLAUDE.md`、`.github/`、`memory/` | 全部 |

### 铁律

1. **CLAUDE.md / docs/ 只在 C 窗口改**——A/B 只读
2. **Memory 文件名不重复**——各窗口按 `{type}_{topic}.md` 命名，MEMORY.md 索引合并时由 C 窗口统一整理
3. **同一个 wheel 不拆两个窗口**——改 API + 改消费者放同一窗口顺序做
4. **合并顺序：下游先合**——C（tooling）→ A（wheels）→ B（project），因为 project 依赖 wheel，wheel 依赖 tooling

### 收窗

**`dispatch_agent.sh --merge-all` 方式**（推荐）：

```bash
# 一键 squash merge 所有 worktree-* 分支 + make check + 清理
wsl bash tooling/session/dispatch_agent.sh --merge-all --push
```

自动执行：squash merge 每个 worktree 分支 → commit → 删除 worktree 和分支 → `make check` → push。

**`claude -w` 手动方式**：无改动时 worktree 自动删除。有改动时合并到主分支：

```bash
# 在主目录
git merge --squash worktree-wheels && git commit
git merge --squash worktree-project && git commit
```

**手动 worktree 方式**（仍可用）：

```bash
git worktree add ../ai-ep-wheels  -b session/wheels
git worktree add ../ai-ep-project -b session/<project>
# 收窗时
git merge session/wheels && git merge session/<project>
git worktree remove ../ai-ep-wheels && git worktree remove ../ai-ep-project
git branch -d session/wheels session/<project>
```

合并后在 C 窗口统一：更新 CLAUDE.md wheel 计数表、整理 MEMORY.md、跑 `make check`。

### 其他并行能力

| 机制 | 用途 | 需要开终端 |
|------|------|-----------|
| `/loop <间隔> <任务>` | 会话内轮询（CI 状态、部署检查） | 否 |
| Cloud Routines（claude.ai/code/routines） | 无人值守定时任务，跑在云端 | 否 |
| `claude --resume <name>` | 恢复已命名的 session | 否 |

### 注意

- 连续 merge + push 会触发 CI `cancel-in-progress` 全取消——合并完统一 push 一次
- worktree 内 `vsf/` 子模块可能需要 `git submodule update`
- 如果只开 2 个窗口，合并 A+C（wheels + infra）为一个即可

## 关键决策（ADR）

| # | 决策 |
|---|------|
| 001 | VSF 为基座 |
| 002 | Wheel sim 优先验证 |
| 003 | 客户代码分类回流 + IP 剥离 |
| 004 | Monorepo，wheels 外置于 vsf/ |
| 005 | VSF 修改走 patches + vsf-addon |
| 006 | ~~新 wheel 前查 VSF 原生~~ (superseded by 011) |
| 007 | VSF 永久基座，不为切 RTOS 做预案 |
| 008 | VSF linux64 不是 kernel-only sim 路径（走 mps2+QEMU，已实证） |
| 009 | Wheel 依赖规则：具体/接口两类，方向只能向下，禁止循环 |
| 010 | 优先采纳开源实现：查 VSF → 联网搜开源 → 都没有才从零写 |
| 011 | Wheel 构建策略：生态驱动，不等项目触发 |

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
- `exp` — 探索性代码（`tooling/experiments/` 下的 tryouts / sandboxes / probe scaffolds）。不是 wheel，不升级到 `wheels/` 之前保持此 type
- `port` — `vsf-addon/` 下的 HAL 驱动 port（MCU 适配层）。scope 用芯片系列小写（如 `gd32f4xx`）
- `project` — `projects/` 下真实项目的代码改动。scope 用项目名小写（如 `flybox`、`drone_light`）
- `tool` — 跨项目可复用的工具脚本，落 `tooling/audit/`、`tooling/wheel/`、`tooling/session/`、`tooling/ci/`、`tooling/fix/`、`tooling/debug/`、`tooling/metrics/`、`tooling/hil/` 等子目录（如 `gd32f4xx`、未来的 HIL 自动化）。注意：`tooling/experiments/` 走 `exp`；`tooling/patches/` 通常归对应 `project`（patch 是为某个项目写的）
- `ci` — CI/CD 配置，落 `.github/workflows/` 与未来其他 CI 系统配置。scope 用 job 名小写（如 `flybox`、`log`），跨 job 结构改动省略

### scope（可选，与目录对齐）

- `wheels/_draft/<name>/` → `_draft/<name>`
- `wheels/<层>/<name>/` → `<层>/<name>`（例：`primitives/log`）
- `projects/<name>/` → `<name>`（例：`flybox`、`drone_light`）
- `tooling/<subdir>/<name>` → `<name>`（例：`gd32f4xx`、`dispatch_agent`）
- `.github/workflows/<job>` → job 名（例：`flybox`、`log`）
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

**禁止加 `Co-Authored-By: Claude ...` 行**——用户偏好（见 `memory/feedback_no_co_authored_by.md`）。Claude Code 系统示例会生成这一行，本仓明确不用。

### 拆分

- 一 commit = 一逻辑改动
- 跨 type 或跨 scope 必拆

### 批量改动的拆分策略（审计/重构/迁移等）

大量改动（如超级审计产出 50+ 项修复）时，按 **type × 逻辑批次** 拆 commit，不需要每个文件或每个 fix 一个 commit：

| 粒度 | 拆法 | 示例 |
|------|------|------|
| **同 type + 同体系** | 合并为 1 个 commit | log 生态 10 个 wheel 的 19 项修复 → 1 commit |
| **同 type + 同层不同 wheel** | 合并为 1 个 commit | L1 driver 3 个 wheel 修复 → 1 commit |
| **跨 type** | 必须拆 | wheel 修复和 CI 修复 → 2 个 commit |
| **跨 scope 但强关联** | 可合并（body 说明） | qmi8658a API 重构 + flybox 消费者适配 → wheel commit 含 consumer 更新，或拆为 wheel + project 两个 |

**实操原则**：
- 每个 commit 能独立 revert 且 diff 可读（<300 行改动为佳）
- summary 列出涉及的 wheel/文件数量和修复类别
- body 按 CRITICAL / MODERATE / MINOR 分级列出修复内容
- 不要为了拆而拆——5 个同类 typo 修复合成 1 个 commit 比 5 个微 commit 有意义

**典型审计 commit 序列**（按 type 分组）：
```
wheel(primitives/log): fix N audit findings across log ecosystem
wheel: fix L0 primitive audit findings
wheel: fix L1 driver audit findings
wheel: fix L2 protocol audit findings
project(flybox): fix N audit findings across tasks and drivers
tool: fix N tooling script audit findings
ci: fix CI pipeline audit findings
docs: fix documentation drift across N files
port(gd32f4xx): fix N HAL audit findings
```

### AI 易踩的坑

- ❌ Conventional Commits 类型（`feat:` / `chore:` / `refactor:` / `build:` / `ci:`）
- ❌ summary 中英混排
- ❌ summary 结尾加句号
- ❌ summary 名词短语无动词（反例 `first wheel — leveled logging`；正例 `add leveled logging wheel`）
- ❌ body 只说 what（反例 `update log.c`；正例 `fix race in log_emit flush`）
- ❌ 加 `Co-Authored-By: Claude ...` 行（Claude Code 默认行为——本仓禁用）

### 新场景处理（escape hatch）

遇到本规范未覆盖的情况（新 type / 新 scope 结构 / 特殊 footer / ...），AI **必须先问用户拍板**，拍板后将新规则追加到本节，再 commit。**不得沉默创造**。

## 当前状态

Sprint 0 + Sprint 0.1（自审改进）已完成：
- ✅ 目录结构（docs/adr / wheels L0-L3 / vsf-addon / tooling/patches）
- ✅ 核心文档（philosophy / wheel-contract / api-conventions / ai-workflow / debugging-playbook）
- ✅ 10 个 ADR（平台决策历史）
- ✅ tooling/patches + apply.sh 骨架
- ✅ vsf-addon 骨架
- ✅ Claude Code 自动记忆（用户背景 + 战略决策 + feedback）
- ✅ 历史评估归档 `_eval/`
- ✅ git 仓库（vsf/ vendored 不 fork）
- ✅ **第一个 wheel：`wheels/primitives/log/`** —— 带等级 + 模块 + 编译期过滤 + 多 sink；12 C 测试 + 3 pytest（含 release 构建编译期消除验证）全绿；作为后续 wheel 的 canonical 例
- ✅ `wheels/WISHLIST.md`（记忆库，不是 TODO）
- ✅ **VSF SIM 投资期探索 2 轮（2026-04-22 日 / 晚）**：(1) linux64 kernel-only 路径证伪（scaffold `tooling/experiments/vsf-tryouts/kernel_test/` 保留作 triage 基线，见 ADR-008）；(2) mps2-an500 bare-metal 首选路径**实证跑通**（scaffold `tooling/experiments/vsf-tryouts/mps2_bare/`，QEMU `-M mps2-an500 -cpu cortex-m7 -semihosting` 下 user_main 正常调度 + semihosting probe 输出 + exit 42 全通）；ADR-008 选项 C 从"选了但没验证"升级为 first-class；debugging-playbook 新增两组 canonical checklist（mps2 首选 + linux64 triage）共 10 条坑

已完成：
- ✅ **第一个真实项目 `projects/flybox/`**（无人机遥测盒子，GD32F405 + FreeRTOS + Air780EG）
  - 阶段 A（结构整合）完成
  - VSF compat 层完成：FreeRTOS API 通过 POSIX shim 运行在 VSF 上，8 个 compat 问题已修，600s HIL soak 0 watchdog reset（2026-04-27）
  - 5 个 wheel 沉淀完成（crc16 / w25qxx / qmi8658a / xgzp6816d / qmc5883p），全部 CI + flybox 真消费
  - 磁力计功能完成：QMC5883P 驱动 + Mahony 9 轴 + 校准（Flash 持久化）
- ✅ **第二个真实项目 `projects/drone_light/`**（无人机 LED 灯控，N32G003 裸机）
  - 轻量整合完成：不做 VSF 迁移（30KB Flash 放不下），保持裸机
  - Wheel 候选标记：WS2812 驱动 (L1)、NCP5623 协议解析 (L2)
- ✅ **GD32F4xx VSF HAL port**（`vsf-addon/hal/driver/gd/GD32F4XX/`）——GPIO/USART/Flash/SPI/I2C 五个外设驱动，flybox HIL 验证通过（2026-04-26）
- ✅ **VSF linux shell POSIX 验证**（`tooling/experiments/vsf-tryouts/linux_shell_size/`）——pthread/sem/recursive-mutex 在 QEMU 实跑通过，开销 +56KB Flash +8KB RAM
- ✅ **GitHub Actions CI**（`.github/workflows/ci.yml`）——增量检测：detect-changes 扫描 git diff，只测改动的 wheel（O(changed) 而非 O(N)）；harness/audit 变更触发全量回归；log + crc16 独立 job（pytest）；flybox 按需触发
- ✅ **开发效率工具链**（`tooling/` 各子目录 + `tooling/test/`）——test_harness.h（统一测试框架）/ test_all.sh / test_downstream.sh / new_wheel.sh（i2c/spi/gpio/onewire/pwm 五种 HAL 模板）/ session_bootstrap.sh / wheel_promote.sh / audit_wheels.py（57 项自动检查 + TEST/RUN 平衡检测 + --graph + --quiet + --json）/ dispatch_agent.sh（worktree agent 派发 + --merge-all 合并）/ build_flash / parse_fault.py / sync_vsf.sh / Makefile（45 目标）/ `.githooks/commit-msg`（7 项格式守卫）/ `.githooks/pre-commit`（二进制拦截）
- ✅ **143+ wheels**（全部 candidate，超级审计 0 open findings）：
  - 完整列表：`ls wheels/primitives/ wheels/drivers/ wheels/protocols/ wheels/modules/`
  - L0 原语 50+：log 生态(10) / crc 族(crc8/crc16/crc32) / 数据结构(ring_buffer/fifo_queue/bitmap/byte_buffer/event_queue) / 数学(fixed_point/moving_average/moving_window/median_filter/pid_controller/mahony_ahrs/mag_cal) / 编解码(base64/hex_codec/json_parser/printf_tiny/cobs/slip) / 系统(state_machine/soft_timer/timeout/retry/debounce/led_pattern/timer_wheel/rate_limiter) / 工具(uuid/endian/hash_fnv/string_utils/lookup_table/checksum/bitfield)
  - L1 驱动 50+：传感器(bmp280/bme280/bme680/sht30/si7021/dht11/ds18b20/bh1750/veml7700/opt3001/tmp102/ina219/hx711/max31855/max31865) / IMU(mpu6050/qmi8658a/lis3dh) / 磁力计(qmc5883p/hmc5883l) / 显示(oled_ssd1306/max7219) / 存储(w25qxx/at24cxx/sd_spi) / 通信(nrf24l01/lora_sx1276/enc28j60/mcp2515) / 输入(gpio_button/rotary_encoder/keypad_matrix/as5600) / 输出(gpio_led/ws2812/servo/stepper/pca9685/drv8833/drv2605) / 扩展(mcp23017/pcf8574/tca9548a) / 其他(ads1115/ads1015/xgzp6816d/apds9960/tcs34725/vl53l0x/mlx90614)
  - L2 协议 25+：mqtt_packet/mqtt5_packet/modbus_rtu/modbus_tcp/coap_packet/http_parser/websocket_frame/at_command/nmea_parser/lorawan_packet/canbus/dns_packet/protobuf_lite/cbor_lite/msgpack/tlv_codec/syslog_packet/cobs/slip/xmodem/ymodem/bencode/crc_any/midi_packet/i2c_scan/ota_ab/ncp5623_parser
  - L3 模块 18+：cli_shell/data_logger/sensor_hub/config_store/error_handler/device_info/task_watchdog/ota_manager/telemetry/button_handler/scheduler/param_table/ring_log/event_bus/flash_journal/command_queue/boot_counter/version_info
- ✅ **HIL 自动化**（`tooling/hil/hil_runner.py`）——封装 build_flash_check.sh，soak 三档（quick 30s / standard 600s / stress 1800s），结果按时间戳存档，--compare 对比历史
- ✅ **项目整合模板**（`templates/project-integration/`）——CLAUDE.md 骨架 + 目录结构指南，从 flybox + drone_light 共同模式提取

未开展：
- 基础 wheels（生态驱动，按领域覆盖度主动构建，见 ADR-011）

## 下一步触发点（Sprint 0 已封版）

**无真实项目触发前不改 `docs/`**。文档规则已经被 flybox + drone_light 两个项目检验，当前无新发现驱动的修改需求。

解冻条件（触发就开工，未触发就别动文档）：

- 用户说"开始第 N 个项目 yyy" → 走 `ai-workflow.md` 任务类型 2
- 用户说"加个 wheel xxx" → 走任务类型 1，**强制先 grep VSF**
- 用户说"项目 zzz 交付了" → 走任务类型 9（沉淀）
- 用户说"审计" → 列维度表选择；"超级审计" → 40 维度三阶段（见 `docs/audit-dimensions.md`）
