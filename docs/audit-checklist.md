# Super Audit Agent Checklist

This file is read by audit agents to avoid re-reporting known issues and to focus on high-value angles.

## Pre-flight

1. Read `docs/known-issues.md` — do NOT re-report anything listed there
2. Read this checklist — follow the angle specified in your prompt
3. Fix confirmed issues and commit (dispatch_agent.sh auto-appends commit instructions)

## Standard edge-case traces (apply to EVERY wheel)

- [ ] **Zero-init struct**: `memset(&dev, 0, sizeof(dev))` then call primary API without `init()`. Must return error, not UB (no `% 0`, no NULL deref).
- [ ] **NULL function pointers in HAL**: For I2C/SPI/GPIO drivers, pass a struct with `write_reg = NULL`. Must return error before dereference.
- [ ] **Max-value inputs**: `UINT32_MAX` for tick counts, `SIZE_MAX` for lengths, `UINT16_MAX` for register addresses.
- [ ] **Empty input**: `len = 0`, empty strings, zero-count arrays.

## Protocol-specific checks

- [ ] **RFC compliance**: Every rejection in the spec ("MUST", "MUST NOT") must have a code guard AND a test
- [ ] **Error code coverage**: For each error enum value, at least one test must trigger it
- [ ] **Crafted input**: Fuzz-like traces — oversized fields, truncated packets, wrong packet type to parser
- [ ] **Reserved values**: Reject reserved enum values, reserved protocol fields

## Fix verification (when reviewing prior fixes)

- [ ] **Overflow guard completeness**: After a guard on `x = a + b`, check if `x` is modified again later (`x += c`)
- [ ] **Comparison matrix**: For any `lines_equal` / `memcmp` logic, list all input-size combinations and trace each
- [ ] **No regression**: Does the fix break any previously-passing scenario?

## Audit angle menu (pick ONE per agent, don't mix)

| Angle | Focus | Best when |
|-------|-------|-----------|
| Generic sweep | Logic errors, overflow, UB | Early rounds (R1-R4) |
| RFC compliance | Spec violations per RFC section | After generic sweeps |
| Error code coverage | Every enum error → test | After guards are added |
| Zero-init safety | BSS/memset(0) edge cases | After API is stable |
| Cross-wheel interaction | Output format vs input assumption | After individual wheels clean |
| Fix regression | Verify prior fixes didn't break | After multiple fix rounds |
| Backlog cleanup | Evaluate accumulated MINORs | After CRITICALs exhausted |
| Test gap closure | Every guard → one test | After guards added without tests |
| -Wconversion | Implicit type narrowing | Final polish |

## Dispatch-based execution

超级审计使用 `dispatch_agent.sh` 派发，不再手动启动 agent：

```bash
# Phase 0: 自动化 gate（基线必须干净）
make check

# Phase 1: 按批次派发 agent（每个 agent 独立 worktree + 自动 commit）
wsl bash tooling/scripts/dispatch_agent.sh \
  --name audit-w1a --budget 1.00 \
  --scope "wheels/primitives/" \
  --prompt "Read docs/audit-checklist.md. Audit W1a (bitfield→kvs). Angle: generic sweep."

wsl bash tooling/scripts/dispatch_agent.sh \
  --name audit-w2 --budget 1.00 \
  --scope "wheels/drivers/" \
  --prompt "Read docs/audit-checklist.md. Audit W2: all L1 drivers."

# 监控各 agent 进展
wsl bash tooling/scripts/dispatch_agent.sh --status

# Phase 2: 全部完成后 squash merge + make check + push
wsl bash tooling/scripts/dispatch_agent.sh --merge-all --push
```

**主窗口角色**：只做调度 + 监控 `--status` + 合并 `--merge-all`，不做修复实现。

**每个 dispatch prompt 自动追加**：
- commit 指令（agent 完成后必须 `git add` + `git commit`）
- scope 限制（`--scope` 指定路径之外不可修改）
