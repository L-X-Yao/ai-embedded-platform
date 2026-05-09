# ADR 005: VSF 上游修改策略（patches + vsf-addon）

**状态**：accepted
**日期**：2026-04-21
**决策人**：@user

## 背景

使用 VSF 过程中会遇到：
1. VSF 上游 bug 需修
2. VSF 上游缺功能需加
3. 需为新 MCU（国民/兆易/先辑等）写 HAL port

问题：这些改动如何与 VSF 上游共存？如何同步 upstream？

## 考虑过的选项

### A. 直接改 vsf/ 源码（最简但最差）
- 优点：改了就改了
- 缺点：上游更新后冲突灾难，完全无法判断"哪些是我们的"

### B. Private fork VSF（公司私有 GitHub/Gitea）
- 优点：真实 git commit 历史、可回溯
- 缺点：多一个 repo 要同步、需要 fork 基建、上游 PR 需提取

### C. Patch 文件 + vsf-addon（选中）✓
- 优点：
  - vsf/ 保持干净（可直接 pull upstream）
  - 每个 patch 是独立可读文件
  - patch 可直接作为 upstream PR
  - 无需额外 repo 基建
- 缺点：
  - patch 文件不如 git commit 友好（无 blame/history）
  - 冲突解决时 patch 可能需要重写

## 决策

**选 C：Patch + Addon 策略**。

### 改 vsf/ 既有文件 → patches

流程：
1. AI 在 vsf/ 改代码 + 调试验证
2. 导出 patch：
   ```bash
   cd <root>
   git diff vsf/ > tooling/ci/patches/NNNN-<slug>.patch
   git checkout vsf/    # 撤回原改动
   ```
3. 更新 `tooling/ci/patches/README.md` 清单
4. 构建前应用：`bash tooling/ci/patches/apply.sh`

### 新增代码 → vsf-addon/

**不修改 vsf/ 文件，而是新增文件**（典型场景：新 MCU port、新组件）：
- 放 `vsf-addon/hal/driver/<vendor>/` 或 `vsf-addon/component/<name>/`
- CMake 在 include VSF 后 `add_subdirectory(vsf-addon/...)`
- **不走 patch 机制**

### 定期同步 upstream

`vsf/` 是 vendor 进母仓的文件（非 submodule / subtree），使用 `tooling/ci/sync_vsf.sh` 同步：

```bash
bash tooling/ci/sync_vsf.sh              # sync to upstream master HEAD
bash tooling/ci/sync_vsf.sh --ref v1.2.3 # sync to specific tag/commit
bash tooling/ci/sync_vsf.sh --dry-run     # preview only
bash tooling/ci/sync_vsf.sh --status      # compare current vs upstream
```

脚本流程（每季度或重要更新时执行）：
1. 获取上游最新到本地缓存 `tooling/.vsf-upstream/`
2. 回退已应用的 patches（`apply.sh --revert`）
3. 替换 `vsf/` 全部内容为上游目标版本
4. 重新应用 `tooling/ci/patches/*.patch`
5. 冲突的 patch 要么改、要么丢弃（上游可能已自己修了）
6. 更新 patches/README 每条状态
7. `make test-wheels` 全平台回归测试
8. 用户审 diff → commit

AI 可主导该流程，最终 merge 人审批。

### 回馈 upstream

通用 bug fix → 整理成 upstream-friendly 版本（英文 commit msg、去公司特定代码）提交 PR 到 `vsfteam/vsf`。被合并后可从我们 `tooling/ci/patches/` 删除。

## 后果

### 好
- vsf/ 始终是干净 upstream 副本，pull 简单
- patches 独立文件，一目了然
- 回馈 upstream 低摩擦
- 无需 fork 基建

### 坏
- patches 多于 15-20 个时维护吃力（考虑升级到 fork）
- 上游大重构时所有 patch 可能需要重做

## 升级信号

当以下任一发生，考虑升级到 fork 方案：
- patches 数量 > 20
- 每季度同步花费 > 8 小时
- 需要跟 upstream 做大量 back-and-forth（分叉工作）

## 何时应重新评估

- 升级信号触发
- VSF 死掉需 AI 接管维护（fork 成为我们的主 vsf/）

## 相关

- ADR-004（Monorepo 结构）
- `tooling/ci/patches/README.md`
- `vsf-addon/README.md`
