# AI 工作流：各类任务的标准路径

进入本平台的 AI，按任务类型走对应路径。**不要跳步**。

## 任务类型 1：新建一个轮子

**触发**：用户说"给我写个 xxx"，或项目中发现需要一个通用组件。

**核心原则**：Wheel 是被动库（见 [architecture.md](architecture.md)）。构建策略：生态驱动（[ADR-011](adr/011-ecosystem-driven-wheel-building.md)）。

1. **查重（wheels 内）**：`ls wheels/` + `ls wheels/_draft/`。已有？去用。
2. **搜索通用开源实现**（按 ADR-010）：**必须用 web search**（不能只凭训练知识），在 GitHub/Gitee 找生产级实现。找到多个候选时：列出选项 + 对比（体积 / 功能 / 维护活跃度）+ **明确推荐**哪个 + 用户做最终决策。有好的 → vendor 进来 + 薄封装，跳到步骤 4。
3. **判断是否适合做 wheel**：需要创建任务/加锁/sleep/中断 → 不是 wheel（见 architecture.md）。
4. **对齐 API（人审批）**：列 5-10 个关键函数签名 + 2 个使用示例，给用户 15 分钟拍板。
5. **Scaffold**：`new_wheel.sh --layer <层> --name <名> [--hal <类型>]`，按 `wheel-contract.md` 搭结构。vendor 类加 `--vendor`（含 `vendor/` + `VENDOR.md`）。
6. **TDD**：先写失败测试 → 实现 → 测试绿。
7. **跑 host sim 验证**。
8. **交回**："draft 就绪，什么时候升 candidate？"

## 任务类型 2：新建一个项目

**触发**：用户说"开始一个新项目：xx"。

1. **追问需求**：目标硬件（MCU 型号）/ 外设需求 / 约束（体积/功耗/实时性）
2. **选 template**：从 `templates/` 找最接近的。没有 → 选最小的或从零（后续抽模板）。
3. **创建**：`projects/<project-name>/`，从 template 拷贝。
4. **写项目 CLAUDE.md**：
   - 项目目标（一段话）
   - 硬件选型
   - 使用的 wheels（引用 `wheels/...`，锁 git tag 版本）
   - 项目特定约定
   - wheel 候选清单（从项目代码识别的可抽取组件）
4.5. **同步 WISHLIST**：项目 CLAUDE.md 的 wheel 候选清单 → `wheels/WISHLIST.md` 对应行触发计数 +1（跨项目 ≥3 升级阈值需要此数据）
5. **配 build**：按硬件调 `VSF_TARGET`、引用 wheels（接入方法见 [`docs/integrating-wheels.md`](integrating-wheels.md)）、Kconfig。
6. **至少 build 一次**（host sim），确保脚手架正常。
7. **交回**：问用户第一个功能。

## 任务类型 3：给现有项目加功能

**触发**：用户说"在 projects/xxx 里加 yy"。

1. **读项目 CLAUDE.md**（如有）。
2. **查 wheels/**：yy 是否已有轮子？
   - 有 → 引用
   - 没有 → **不要偷偷新建在项目里**，先问用户"要不要顺便做成 wheel"
3. **读现有代码结构**。
4. **写代码 + 测试**（同时写）。
5. **跑测试**。
6. **如需改 wheel**：先在 wheel 仓改、补测试，再在项目里引用新版。

## 任务类型 4：修 bug

**触发**：用户说"xxx 不对" / "test 挂了" / "板子行为异常"。

1. **复现成 failing test**。没能复现就不开始改。
2. **读完整错误信息**（不只头行）。
3. **查 `debugging-playbook.md`**：是已知坑吗？
4. **查 Claude Code 自动记忆**：用户之前指出过类似？
5. **二分定位**：注释 / 撤 commit / 简化，找引入 bug 的变更。
6. **写最小复现 test → 修 → 全测试回归**。
7. **更新 playbook**：记录这个坑，让下一个 AI 秒解。

## 任务类型 5：技术选型（例：MQTT 选哪个）

**触发**：用户说"用 A 还是 B 好" / "选哪个 MQTT 库" / "VSF 和 X 哪个合适"。

**这是用户 10% 时间里最典型的一类**——AI 分析 + 人拍板。

1. **列候选**：通常 2-4 个（不要超过 5，选择疲劳）。
2. **建立评估维度**（不纸面，可量化）：
   - 代码大小 / RAM 占用 / 移植性
   - 功能完备度（vs 需求）
   - 许可证（Apache/BSD/MIT/GPL）
   - 维护活跃度（近 12 月 commits、最新 release）
   - 与 VSF 集成难度
   - 已有平台 wheels 的依赖（复用还是冲突）
3. **每个候选至少 sim 跑 hello-world**：纸面评估常错。
4. **对比表 + 明确推荐**：不给"都可以"。
5. **审批**：用户看对比表 15 分钟拍板。
6. **记 ADR**：选型结果写 `docs/adr/NNN-<slug>.md`，记录为何这样选、何时应重评估。

## 任务类型 6：移植（换 MCU）

**触发**：wheel 或项目需要跑在新芯片上（比如国民技术/兆易/先辑）。

1. **查 VSF 是否有 port**：`ls vsf/source/hal/driver/`。
2. **如无 port**：评估是否值得做（用户决策）。做的话在 `vsf-addon/hal/driver/<vendor>/` 按 VSF HAL 接口写。
3. **按 VSF 模板写 port 代码**（参考现有 port，如 `vsf/source/hal/driver/ST/`）。
4. **sim 先能跑**（用 mock hal）。
5. **真机验收**：必须人工烧板 + 串口验证，写入 README 的"支持的平台"。
6. **wheel 若用了 MCU 特定 API**：回头重构为 HAL 调用。

## 任务类型 7：客户代码抽取（项目交付后）

**触发**：项目交付完，识别通用部分回流 wheels/。

1. **扫项目源码**找候选（可复用的函数/模块）。
2. **分类**（按 `docs/adr/003`）：
   - 可回流：驱动封装 / 协议栈 / 通用算法 / 无业务的基础工具
   - 不可回流：业务逻辑 / 客户 IP / VID/PID / 密钥 / 标定数据
   - 需剥离后回流：配置可配置 / 硬编码参数 / 接口依赖客户服务器
3. **剥离客户特定信息**：逐行过一遍，确认无泄漏。
4. **API 提案**：给用户看（任务类型 1 的 API 审批）。
5. **抽到 `wheels/_draft/<name>/`**。
6. **在项目里改为引用 wheel**：确保项目仍然能通过回归测试。
7. **更新项目 CLAUDE.md**：记录新抽取的 wheel。

## 任务类型 8：VSF 上游修改

**触发**：需要改 `vsf/` 既有文件（修 bug / 加功能）。

1. **在 `vsf/` 里直接改 + 调试验证**。
2. **导出 patch**：
   ```bash
   cd <root>
   git diff vsf/ > tooling/patches/NNNN-<slug>.patch
   git checkout vsf/      # 撤回原改动，保持 vsf/ 干净
   ```
3. **更新 `tooling/patches/README.md`**：
   - Patch # / 文件 / 描述 / 发现于哪个项目 / 日期
   - 是否提交 upstream PR / upstream 状态
4. **考虑提 upstream PR**：通用 bug fix 建议提，减少长期维护负担。
5. **验证 patch 可重现**：`bash tooling/patches/apply.sh` 能干净应用。

### 新增独立代码（非 patch）

**不改 `vsf/` 文件，而是加新文件（如新 MCU port、新组件）** → 放 `vsf-addon/` 对应子目录。CMake 里在 include VSF 之后 `add_subdirectory(vsf-addon/...)`。

## 任务类型 9：沉淀（项目结束后）🔑

**触发**：项目交付完毕，准备归档。

**这是整个平台复利机制的核心**，不做沉淀 = 这个项目的知识归零。

### 阶段 1：AI 自主扫（1-2 小时）

1. **扫项目代码**找"可抽 wheel 的候选"：
   - 这个函数/模块在其他项目里会用到吗？
   - 剥掉业务逻辑后剩下什么？
   - 符合哪层（L0/L1/L2/L3）？
2. **每候选产出 API 提案**：
   - 5-10 个函数签名
   - 2 个使用示例（包括项目内的原用法）
   - 符合 `api-conventions.md`
   - 附 "Why not VSF native?" 节草稿

### 阶段 2：人审批（15 分钟/wheel）

3. **发提案给用户**：列表 + 每项的 API 草案。
4. **用户拍板**：通过 / 改成 X / 不抽。

### 阶段 3：AI 实现（1-3 小时）

**沉淀闭环 = 三件套，缺一不可（由 `tooling/audit/audit_wheels.py` + `wheel-audit` CI job 强制执行）**：

(1) **wheel 代码 + 测试**

5. **按拍板后 API 实现**在 `wheels/_draft/<name>/`（或合适的 L 层目录）。
6. **写测试**（sim 级别，不照抄项目集成测试）。
7. **写 README**（按 wheel-contract §3.1 的 draft 要求）。
8. **跑 sim 验证**全绿。

(2) **CI job**

9. **CI 自动覆盖**：标准 wheel（有 `test/CMakeLists.txt`）被 CI detect-changes 自动发现，无需手动编辑 ci.yml。仅含 pytest 的 wheel（如 log / crc16）需添加独立 job。push 后 CI 全绿才算 step 完成。

(3) **真项目消费**

10. **在 `projects/<name>/CMakeLists.txt` 引用 wheel**（include path + add to sources），项目原代码改为调用 wheel API，项目测试仍通过。

⚠️ **wheel commit ≠ 沉淀闭环**：仅做 step (1) 而忘 (2)/(3) → 产生 floating wheel（代码看似 done 但没人跑、没人用，腐烂只是时间问题）。2026-04-25/26 曾出现 `w25qxx` + `qmi8658a` 两个 orphan wheel 持续 1-2 天才被发现，现已工具化拦截。

### 批量沉淀（多 wheel 并行）

当一个项目同时沉淀 3+ 个 wheel 时（如 flybox 一次沉淀 5 个），走并行路径：

1. **AI 一次性扫完所有候选**，统一列表给用户
2. **用户批量拍板**（可以一次通过多个 API 提案）
3. **AI 按依赖顺序实现**：先做被依赖的 wheel（如 crc16），再做依赖它的（如 w25qxx）
4. **CI job 批量追加**：一次性加完所有 wheel 的 CI 条目，避免连续 push 触发 cancel-in-progress
5. **消费者适配一次性做完**：项目代码改为引用所有新 wheel，一次构建验证
6. **拆 commit 按 type × 批次**：全部 wheel 代码合 1-2 个 commit，CI 改动 1 个，项目适配 1 个

**不要**每个 wheel 走一遍完整的单 wheel 流程——那会产生 N×5 个 commit 和 N 轮 CI，效率极低。

### 阶段 4：更新交叉资料（AI 做）

10. **更新 `docs/debugging-playbook.md`**：项目踩的坑。
11. **更新 `docs/adr/`**：如有架构级决策。
12. **更新 Claude Code 自动记忆**：跨项目适用的经验/偏好。
13. **更新 `wheels/README.md`** 目录清单。

### 阶段 5：人终审（10 分钟）

14. **过 diff 一眼**：看新 wheel 代码、README、项目修改。
15. **归档**：项目在 `projects/` 里打 git tag。

## 任务类型 10：代码审查 / 重构

**触发**：用户说"过一下 xxx 代码" / "这段写得乱"。

1. **读代码全文**（不只 diff）。
2. **对照**：`wheel-contract.md` + `api-conventions.md` + 项目 CLAUDE.md
3. **问题分级**：
   - **必须改**（bug / 违反约定 / 测试缺失）
   - **建议改**（命名 / 注释 / 结构）
   - **可选**（风格偏好）
4. **不擅自大重构**。提议让用户拍板。

## 任务类型 11：平台自省（自发或用户触发）

**触发**：
- 用户说"审计"或"超级审计"
- 完成一个 Sprint / 里程碑后
- 每 ~10 个 wheel 后（自发）

**目的**：平台文档/代码/流程之间**不一致 / 过时 / 腐烂**的地方，被积累前发现。

**完整维度清单和执行协议**：见 [`docs/audit-dimensions.md`](audit-dimensions.md)（40 个维度，三阶段执行（自动→人工→记录））。

**交互协议**：

| 指令 | 行为 |
|------|------|
| `审计` | 列出维度表，等用户选择 |
| `审计 <维度名>` | 单维度审查 + 修复 |
| `超级审计` | 全维度并行启动，按完成顺序汇报 + 修复 |

**执行模式：`dispatch_agent.sh` 自动派发**

超级审计使用 `dispatch_agent.sh` 将每个维度/批次派发到隔离 worktree：

```bash
# Phase 0：自动化 gate
make check

# Phase 1：按批次派发 agent（每个 agent 独立 worktree + 独立 commit）
wsl bash tooling/session/dispatch_agent.sh \
  --name audit-w1a --budget 1.00 \
  --scope "wheels/primitives/" \
  --prompt "审计 W1a: bitfield→kvs，按 audit-checklist.md 角度检查"

wsl bash tooling/session/dispatch_agent.sh \
  --name audit-w2 --budget 1.00 \
  --scope "wheels/drivers/" \
  --prompt "审计 W2: L1 驱动层全部 wheel"

# 监控进度
wsl bash tooling/session/dispatch_agent.sh --status

# Phase 2：一次性合并所有 worktree 分支（squash merge）
wsl bash tooling/session/dispatch_agent.sh --merge-all --push
```

**主窗口角色**：调度中心——派发任务、监控 `--status`、合并结果、更新 memory。**不做长线实现**。

**输出规范**：每维度按 CRITICAL / MODERATE / MINOR 分级汇报，附文件:行号和修复方式。用户拍板后批量执行 + 分类 commit。

**反面清单**：
- 不要自己决定改——必须等用户审批
- 不要一次报 30 项——分类 + 合并相似的
- 不要报"可以考虑" / "看你喜好"——要有明确推荐（用户偏好：给具体建议，不要罗列选项让用户选）

---

## 反模式（不要做）

### 1. 静默越界

任务是"加 A"，顺手重构了 B。**问，不要做**。

### 2. 假"完成"

"编译通过 = 完成" 不成立。**必须跑测试**。

### 3. 猜测代替阅读

"我觉得 VSF 的 xxx 应该是这样" → 不，**读源码或 canonical example**。

### 4. 自造轮子

先查 `wheels/` + `vsf/source/`。

### 5. 跳过踩坑记录

解决了 1 小时的问题没写进 playbook = 下次重花 1 小时。

### 6. 自作主张改上游

动 `vsf/source/*` → 走任务类型 8 流程，不要直接覆盖。

### 7. 沉淀跳过 API 审批

AI 自己定 API 结构 = 长期风格漂移。**必须等人拍板**。

### 8. 盲信并行 agent 结论

多 agent 并行审计的假阳性率约 **20-30%**。agent 报"发现问题"后，必须先 `grep` 验证关键 token 确实存在，再开始修。不验证就修 = 引入不必要的改动。

### 9. 连续 push 触发 CI 全取消

快速连续 push（如审计后 5 个 commit 连推）会被 `cancel-in-progress` 全部取消。**改完统一 push 一次**，或合并后空 commit 重触发。

### 10. scaffold 缺陷传播

`new_wheel.sh` 模板中的问题会传播到所有用它生成的 wheel。修 29 个 wheel 不如修 1 个模板。**发现 scaffold 缺陷优先修模板**，再决定是否回溯修已有 wheel。

---

## 任务类型 12：发布（打版本标签）

**触发**：用户说"发布"或达到里程碑（新项目完成、大批 wheel 稳定等）。

**版本规则**（vMAJOR.MINOR.PATCH）：
- **MAJOR**：wheel API 有 breaking change 或架构大调整
- **MINOR**：新增 wheel、新功能、重要修复
- **PATCH**：bug 修复、文档更新、工具改进

1. `make check` — 确保测试 + 审计干净
2. `bash tooling/wheel/release.sh --dry-run v<X.Y.Z>` — 预览 changelog
3. 用户确认版本号
4. `bash tooling/wheel/release.sh v<X.Y.Z>` — 创建 tag + 更新 CHANGELOG.md
5. `git add CHANGELOG.md && git commit` + `git push && git push --tags`

项目锁定平台版本：在项目 CLAUDE.md 记录 `平台版本: v<X.Y.Z>`。

---

## 跨切面：Wheel 变更影响分析

**不是任务类型，而是修改任何已有 wheel 时必须执行的协议。** 适用于任务类型 1（改已有 wheel）、3（加功能需改 wheel）、4（修 wheel bug）、9（沉淀改 API）。

### 何时触发

只要改了 `wheels/<layer>/<name>/` 下的 `.h` 或 `.c` 文件，在 commit 前执行。

### 执行步骤

```bash
# 1. 查下游消费者
make downstream W=<wheel-name>      # 或 --list 只看不跑

# 2. 如果有 wheel 消费者 → 脚本自动构建 + 测试
#    如果有 project 消费者 → 脚本报告，手动判断

# 3. 全部绿 → commit
#    有失败 → 在同一个 commit 或紧接的 commit 中修复消费者
```

### Breaking change 处理

当 wheel 的公开 API 签名改变（函数增删改参数、struct 字段变化、错误码含义变化）：

1. **先列出所有下游**：`bash tooling/test/test_downstream.sh --list <name>`
2. **评估影响范围**：
   - 只有 test mock 引用 → 低风险，直接改
   - wheel-to-wheel 依赖 → 同 commit 修上下游
   - project 消费者 → 告知用户，协商时机
3. **改的顺序**：先改 wheel API + 测试 → 再改所有 wheel 消费者 → 最后改 project 消费者
4. **commit 策略**：wheel API + wheel 消费者可合一个 commit（body 说明 breaking change）；project 消费者单独 commit（type=project）

### CI 自动保障

CI detect-changes 自动检测跨 wheel 依赖：当上游 wheel 变更时，下游 wheel 的测试也会被包含在矩阵中。无需手动添加依赖映射。

### Non-breaking change（bug fix、内部重构）

公开 API 不变，只改内部实现：
1. `make downstream W=<name>` 验证下游仍然通过
2. 直接 commit，不需要改消费者

---

## 主窗口调度模式

大型任务（超级审计、批量沉淀、跨 wheel 重构）使用**主窗口调度 + 工作窗口执行**模式：

### 主窗口职责（调度中心）

1. **Phase 0**：`make check` 确保基线干净
2. **派发**：用 `dispatch_agent.sh` 为每个子任务创建隔离 worktree agent
3. **监控**：`dispatch_agent.sh --status` 查看各 agent 进展
4. **合并**：`dispatch_agent.sh --merge-all` 一次性 squash merge 所有完成的 worktree 分支
5. **验证**：合并后 `make check` 确认一致性
6. **收尾**：更新 memory、更新 CLAUDE.md 计数、`git push`

**主窗口不做长线实现**——实现全部派发给工作 agent。主窗口只做决策、调度、合并、记录。

### 工作 agent 职责

- 在 `dispatch_agent.sh` 创建的隔离 worktree 中工作
- 只修改 `--scope` 指定的路径
- 完成后自动 commit（dispatch 脚本在 prompt 中追加 commit 指令）
- 不推送——由主窗口统一合并后推送

### 合并策略

```bash
# squash merge：每个 worktree 分支合并为主分支上的一个 commit
wsl bash tooling/session/dispatch_agent.sh --merge-all

# 合并后自动运行 make check
# 全部通过后推送
wsl bash tooling/session/dispatch_agent.sh --merge-all --push
```

使用 `--squash` 而非普通 merge，避免工作 agent 的中间 commit 进入主分支历史。

---

## 与用户协同的节奏

**好节奏**：
- AI 每 15-30 分钟阶段性报告（做什么、下一步什么）
- 分叉时先问再做
- 完成一个 wheel / 功能后主动说"请 review"

**坏节奏**：
- 默默跑 1 小时，最后丢一大段"我做完了"
- 遇问题不问、自试 N 种方法
- 只有结果没有过程

---

## 可用工具脚本（`tooling/`）

| 脚本 | 用途 | 常用参数 |
|------|------|---------|
| `tooling/test/test_all.sh` | 统一跑所有 host 测试 | `<name>` (positional), `--changed`, `--list`, `--coverage`, `--fail-fast` |
| `tooling/test/test_downstream.sh` | Wheel 下游消费者回归测试 | `--all`, `--list` |
| `tooling/wheel/new_wheel.sh` | Wheel 脚手架生成器 | `--layer`, `--vendor` |
| `tooling/session/bootstrap.sh` | AI 会话上下文初始化（替代 5-8 个 tool call） | `--short`（仅 git 状态） |
| `tooling/wheel/wheel_promote.sh` | Wheel 阶段升级（draft→candidate→stable） | `<wheel-path>` |
| `tooling/audit/audit_wheels.py` | CI 一致性审计（59 项检查） | `--graph`, `--quiet`, `--json` |
| `tooling/audit/arm_size_audit.sh` | ARM 交叉编译体积报告 | `--csv`, `--markdown` |
| `tooling/ci/update_wheel_inventory.sh` | 自动更新 CLAUDE.md wheel 计数 | `--apply` |
| `tooling/wheel/release.sh` | 平台版本标签 + CHANGELOG | `--list`, `--current`, `--dry-run` |
| `tooling/audit/audit_fix.py` | 自动修复机械审计发现 | `--apply`, `--only <类别>`, `--wheel <name>` |
| `tooling/fix/batch_fix.sh` | 批量跨 wheel sed/python 变换 | `--sed`, `--py`, `--all`, `--dry-run` |
| `tooling/ci/commit_suggest.sh` | 从 staged diff 生成 commit message | 无 |
| `tooling/audit/doc_drift_check.py` | 检查文档数字断言 vs 实际值 | `--fix` |
| `tooling/wheel/find_all_refs.sh` | 搜索 wheel 在代码+文档的全部引用 | `--code-only`, `--docs-only` |
| `tooling/ci/sync_vsf.sh` | 同步 vsf/ 到上游 | `--status` |
| `tooling/metrics/platform_metrics.sh` | 平台健康指标快照 + 对比 | `--save`, `--compare`, `--history` |
| `tooling/test/test_scaffold.sh` | 验证 7 种 scaffold 模板编译 | 无 |
| `tooling/session/dispatch_agent.sh` | 在隔离 worktree 派发 Claude Code agent | `--name`, `--budget`, `--prompt`, `--scope`, `--wait`, `--merge-all [--push]`, `--status` |
| `tooling/session/handoff.sh` | 生成 session 交接状态文档 | 无 |
| `tooling/metrics/platform_metrics_impl.py` | 平台指标采集实现（被 platform_metrics.sh 调用） | 内部使用 |

### 工具决策树

| 我想要… | 用这个 |
|---------|--------|
| 开始新 session | `make bootstrap` |
| 建一个新 wheel | `tooling/wheel/new_wheel.sh --layer X --name Y [--hal Z]` |
| 跑所有测试 | `make test`（或 `make test-fast` 快速迭代） |
| 只测改了的部分 | `make test-changed` |
| 测某个具体 wheel | `bash tooling/test/test_all.sh <name>` |
| 改了 wheel 后查下游影响 | `make downstream W=<name>` |
| commit 前全面验证 | `make check`（= test + audit） |
| 看 commit message 该怎么写 | `bash tooling/ci/commit_suggest.sh` |
| commit + push 一步到位 | `make ship M="message"` |
| 审计发现了机械问题要批量修 | `make audit-fix-apply` |
| 跨多个 wheel 做同类文本替换 | `tooling/fix/batch_fix.sh --sed 's/old/new/g' --all` |
| 改了 wheel 名想找所有引用 | `tooling/wheel/find_all_refs.sh <name>` |
| 检查文档数字有没有过时 | `python3 tooling/audit/doc_drift_check.py`（或 `--fix`） |
| 升级 wheel 从 draft 到 candidate | `tooling/wheel/wheel_promote.sh _draft/X primitives/X` |
| 查 ARM 二进制体积 | `wsl bash tooling/audit/arm_size_audit.sh --markdown` |
| 更新 CLAUDE.md 的 wheel 计数 | `make inventory` |
| 打版本标签 | `tooling/wheel/release.sh v1.0.0` |
| 查当前版本 | `tooling/wheel/release.sh --current` |
| 看 wheel 依赖关系图 | `make graph` |
| 同步 VSF 上游 | `make sync-vsf` |
| 清理构建残留 | `make clean` |
| 并行派发多个 agent | `tooling/session/dispatch_agent.sh --name X --budget N --prompt "..."` |
| 查看派发 agent 状态 | `tooling/session/dispatch_agent.sh --status` |
| 合并所有 agent 结果 | `tooling/session/dispatch_agent.sh --merge-all [--push]` |
