# ADR 004: Monorepo 结构

**状态**：accepted
**日期**：2026-04-21
**决策人**：@user

## 背景

平台包含多个逻辑组件：VSF 上游、wheels、templates、projects、docs、tooling 等。可选的仓库组织方式多种。

## 考虑过的选项

### A. 每个组件独立 repo
- 优点：组件边界清晰、独立版本管理
- 缺点：跨组件修改要协调多 repo、团队小时管理负担重

### B. Monorepo（单一私有仓库）✓
- 优点：
  - 跨组件修改原子性 commit
  - 单一 git clone 就齐了
  - wheel 版本锁定通过 git commit hash 即可
  - 小团队（2-3 人）管理最简
- 缺点：
  - 仓库体积大（含 VSF 源码）
  - 权限控制粒度粗（全仓或无）

### C. 私有 fork VSF，wheels 嵌入 fork
- 优点：彻底一个代码树
- 缺点：
  - 上游同步难（我们的改散在 VSF 树各处）
  - 锁死 VSF
  - 许可证边界模糊（我们代码混入 Apache 2.0 树）
  - 上游贡献需额外抽取工作

## 决策

**选 B：Monorepo，wheels 外置于 vsf/**。

目录：
```
ai-embedded-platform/    (整个私有 monorepo)
├── vsf/                 (VSF upstream 副本，不 commit 我们的改动)
├── vsf-addon/           (新增代码，如新 MCU port)
├── wheels/              (L0-L3 可复用模块)
├── templates/           (项目模板)
├── projects/            (真实项目)
├── tooling/patches/     (对 vsf/ 的修改以 patch 形式)
├── docs/                (平台文档)
└── memory/              (AI 跨对话记忆)
```

### 为何不 fork VSF
1. **上游同步容易**：vsf/ 干净，`git pull upstream` 直接能合
2. **许可证清晰**：我们代码 vs Apache 2.0 VSF 边界明确
3. **上游贡献容易**：patches 独立文件可直接 PR
4. **心智简单**：vsf/ 是 vendor 库，wheels 是我们的

### VSF 修改策略
- 改 `vsf/` 既有文件 → 导出为 `tooling/patches/NNNN-*.patch`，撤回 vsf/ 原改
- 新增独立代码 → 放 `vsf-addon/`
- 见 ADR-005

## 后果

### 好
- 团队齐活：单 clone、单 build、单 issue tracker
- 跨 wheel + 项目 + vsf 修改可原子
- 版本管理简单：git tag 就够

### 坏
- 仓库大（几百 MB 起）
- 权限粗（谁能看就看全部）
- `git log` 噪音多（跨组件混合）

## 当前仓库情况

仓库位置：本地 `D:\temp\project\ai-embedded-platform\`（未来推到公司私有 Gitea/GitLab）

### 分支策略
- `main` 干线
- 短 feature branch，合并后删
- **不**搞项目长期分支（项目隔离在 `projects/<name>/` 子目录）

### wheel 版本
- 用 git tag：`wheel/<name>/v0.1.0`
- 每项目在 CLAUDE.md 锁定用的 tag
- wheel 破坏性更新 → major +1 + 写 MIGRATION.md

## 何时应重新评估

- 仓库大小 > 10 GB 影响 clone
- 超过 10 人团队，权限控制成瓶颈
- 某个 wheel 需要开源（可单独抽到独立 public repo）

## 相关

- ADR-005（VSF 上游修改策略）
- `tooling/patches/README.md`
