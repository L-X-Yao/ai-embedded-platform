# ai-embedded-platform

[![CI](https://github.com/L-X-Yao/ai-embedded-platform/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/L-X-Yao/ai-embedded-platform/actions/workflows/ci.yml)

AI 驱动的嵌入式开发母仓库。2-3 人团队 + AI 协作，人管需求/架构/终审（10%），AI 做实现/测试/文档/配置（90%）。

## 基座

[VSF](https://github.com/vsfteam/vsf)（Versaloon Software Framework）—— 永久基座，vendored 在 `vsf/`。

## 目录结构

```
├── docs/                 核心文档 + 8 个 ADR
├── vsf/                  VSF 上游源码（不直接改）
├── vsf-addon/            新增代码（MCU port / 组件）
├── wheels/               可复用轮子（L0 原语 → L3 模块）
│   ├── primitives/       L0：纯软件工具
│   ├── drivers/          L1：硬件驱动
│   ├── protocols/        L2：协议 / 服务
│   ├── modules/          L3：功能模块
│   └── _draft/           孵化区
├── templates/            项目起步模板
├── projects/             真实项目
└── tooling/              patches / HIL / 脚本 / 实验
```

## 环境要求

- Windows 11 + WSL2 Ubuntu 24.04
- WSL 内：gcc 13、cmake、ninja、pytest、pexpect
- QEMU（`qemu-system-arm`，用于 mps2-an500 bare-metal sim）

## 快速开始

```bash
# 在 WSL 中
cd /mnt/d/temp/project/ai-embedded-platform

# 跑 log wheel 测试（当前唯一 wheel）
cd wheels/primitives/log
cmake -GNinja -S . -B build && cmake --build build
./build/log_test

# pytest 回归
python3 -m pytest test_log.py -v
```

## 验证路径

首选 sim 路径：**mps2-an500 + QEMU**（Cortex-M7 bare-metal + semihosting），已实证跑通。

```bash
qemu-system-arm -M mps2-an500 -cpu cortex-m7 -nographic -semihosting -kernel firmware.elf
```

linux64 kernel-only 路径已证伪，保留 triage 用途。详见 ADR-008。

## 关键文档

| 文档 | 内容 |
|------|------|
| `CLAUDE.md` | AI 工作指令（零号铁律、commit 规范、任务流程） |
| `docs/philosophy.md` | 10/90 人-AI 分工愿景 |
| `docs/wheel-contract.md` | 轮子合格定义 + 升级流程 |
| `docs/ai-workflow.md` | 11 类任务标准路径 |
| `docs/debugging-playbook.md` | 踩坑指南 + mps2 checklist |
| `docs/adr/` | 8 个架构决策记录 |
