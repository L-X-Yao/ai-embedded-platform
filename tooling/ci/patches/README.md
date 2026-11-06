# tooling/ci/patches/

对 `vsf/` 上游源码的修改以 patch 文件形式维护。

## 策略

见 `docs/adr/005-vsf-upstream-modification-policy.md`。核心：

- `vsf/` 保持上游干净副本，不直接 commit 我们的改动
- 所有对 `vsf/` 既有文件的修改导出为 patch 文件放这里
- 新增文件（如新 MCU port）不用 patch，直接放 `vsf-addon/`
- 构建前执行 `bash apply.sh` 应用所有 patches

## 当前 patch 清单

| # | 文件 | 描述 | 发现于 | 日期 | upstream PR | 状态 |
|---|------|------|-------|------|-------------|------|
| （暂无） | | | | | | |

## 新增 patch 流程

1. 在 `vsf/` 里直接修改代码，调试验证通过
2. 导出 patch：
   ```bash
   cd <root>   # 即 ai-embedded-platform/
   git diff vsf/ > tooling/ci/patches/NNNN-<slug>.patch
   git restore vsf/   # 撤回原始改动，保持 vsf/ 干净
   ```
3. 更新本 README 的清单表
4. 验证 patch 可应用：
   ```bash
   bash tooling/ci/patches/apply.sh
   cd vsf && <build test>
   git checkout vsf/   # 再次撤回，等下次构建再应用
   ```
5. 考虑是否提 upstream PR（通用 bug fix 建议提）

## 应用 patches

```bash
bash tooling/ci/patches/apply.sh
```

构建前必须应用。注意 `vsf/` 当前状态：
- 干净 upstream → apply 后带修改
- 已应用过 → 重复 apply 会 warning 但不会错

## 定期同步 upstream（每季度或重要更新时）

`vsf/` 是 vendor 进母仓的文件（非 submodule），同步使用专用脚本：

```bash
# 查看当前版本 vs 上游最新
bash tooling/ci/sync_vsf.sh --status

# 预览变更（不写入）
bash tooling/ci/sync_vsf.sh --dry-run

# 同步到 upstream master HEAD
bash tooling/ci/sync_vsf.sh

# 同步到指定 tag / commit
bash tooling/ci/sync_vsf.sh --ref v1.2.3
```

脚本自动处理：
1. 缓存上游 clone 到 `tooling/.vsf-upstream/`（首次慢，后续增量 fetch）
2. 回退已应用的 patches
3. 替换 `vsf/` 全部内容
4. 重新应用 patches（冲突时报告 `.rej` 文件）
5. 打印 diff 摘要，**不自动 commit**

遇冲突时：
- 看 upstream 是否已修掉问题 → 删除该 patch
- 否则：手动解冲突 → 重新导出 patch → 更新此 README

## 与 vsf-addon/ 的区别

| 类型 | 走哪里 | 举例 |
|------|-------|------|
| 修改 vsf/ 既有文件 | `tooling/ci/patches/` | 修 `vsf_eda.c` 某处 bug / 加一个参数 |
| 新增独立文件 / 目录 | `vsf-addon/` | 新 MCU 的 HAL port / 新组件 |

## 回馈 upstream

patches 如果是通用 bug fix 或有价值功能：
1. AI 整理英文 commit message（去公司特定说明）
2. 参考 vsfteam/vsf 的 CONTRIBUTING（如有）
3. 提交 PR 到 `https://github.com/vsfteam/vsf`
4. 更新本 README 的 upstream PR 列
5. 被合并后下次 upstream 同步可删除该 patch
