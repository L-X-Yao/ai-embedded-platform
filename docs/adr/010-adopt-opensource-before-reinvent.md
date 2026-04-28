# ADR 010: 优先采纳开源实现，不重造

**状态**：accepted
**日期**：2026-04-28
**决策人**：@user

## 背景

平台 wheel 来源目前只有三条路径：
1. VSF 原生（ADR-006）
2. 从项目抽取（任务类型 7/9）
3. 从零写

缺第四条：**采纳 GitHub/Gitee 上已有的优质开源实现**。

典型场景：需要 KV 存储，从零写一个简单版本远不如采纳 FlashDB（生产级、磨损均衡、掉电安全）。重造一个更差的版本违反平台效率原则。

## 考虑过的选项

### A. 只用 VSF 原生或从零写

- 优点：零外部依赖，完全可控
- 缺点：重造已有优质实现，质量更差，时间浪费

### B. 在查 VSF 之后增加"查开源"步骤（选中）

- 优点：获得生产级实现，AI 时间用在集成而非重造
- 缺点：需要维护 vendor 代码的更新

## 决策

**选 B：新建 wheel 的决策流增加"查开源"步骤。**

### 决策流（完整）

```
需要某个能力
  1. 查 wheels/ 已有          ← 已有（查重）
  2. grep VSF 原生            ← 已有（ADR-006）
  3. 联网搜索开源优质实现      ← 新增
     → 质量够？（有测试、活跃维护、生产使用）
     → 体积可接受？（Flash/RAM 预算）
     → 有 → vendor 进来 + 薄封装
  4. 以上都无 → 从零写
```

### 关键规则

1. **必须联网搜索**，不能只凭 AI 训练知识判断有没有好的开源实现。训练数据有截止日期，开源社区持续演进。
2. **不做协议过滤**——本仓是私有仓库，不受开源协议约束。
3. **vendor 模式**——将上游源码拷贝进 wheel 目录，不用 git submodule（保持 monorepo 自包含）。
4. **薄封装**——对外 API 遵守 `api-conventions.md`，内部调用上游原始 API。
5. **不改上游代码**——`vendor/` 目录保持原始状态。需要适配的逻辑写在 wrapper 层。

### Vendor wheel 目录结构

```
wheels/<layer>/<name>/
├── README.md           "Why not VSF native?" + "Why this upstream?"
├── <name>.h            平台 API 封装（遵守 api-conventions）
├── <name>.c            薄 wrapper
├── vendor/             上游源码（干净拷贝）
│   ├── LICENSE         保留（尊重作者）
│   └── ...
├── VENDOR.md           上游 URL + 版本/commit + 更新方法
└── test/
    ├── CMakeLists.txt
    └── test_<name>.c
```

### VENDOR.md 模板

```markdown
# Upstream

- **Project**: <名称>
- **URL**: <GitHub/Gitee URL>
- **Version**: <tag 或 commit hash>
- **Vendored**: <日期>

## Update procedure

1. Download/clone upstream at target version
2. Copy <specific dirs/files> into vendor/
3. Run tests: `cd test && cmake ... && ./test_<name>`
4. Update this file with new version/date
```

## 后果

### 好
- 平台获得生产级组件，质量高于自写
- AI 时间花在集成和测试，不是重造
- 上游更新时只需替换 vendor/ + 跑测试

### 坏
- vendor 代码需要跟踪上游更新（但频率低，嵌入式库稳定后很少改）
- 调试时需要读上游代码（但好的库有文档和测试）

## 何时应重新评估

- 仓库从私有转公开 → 需要评估 vendor 代码的协议兼容
- vendor 代码体积超过 500KB → 考虑是否需要裁剪策略

## 相关

- ADR-006（优先 VSF 原生）
- `docs/ai-workflow.md` 任务类型 1
