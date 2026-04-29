# ai-embedded-platform

[![CI](https://github.com/L-X-Yao/ai-embedded-platform/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/L-X-Yao/ai-embedded-platform/actions/workflows/ci.yml)

AI 驱动的嵌入式开发母仓库。2-3 人团队 + AI 协作，人管需求/架构/终审（10%），AI 做实现/测试/文档/配置（90%）。

## 平台现状

- **19 个 candidate wheels**（L0 原语 7 / L1 驱动 7 / L2 协议 5）
- **2 个真实项目**：flybox（无人机遥测盒子）/ drone_light（无人机 LED 灯控）
- **6 个 CI jobs / 22 个测试实例**，push 自动触发
- **基座**：[VSF](https://github.com/vsfteam/vsf)（永久基座，vendored 在 `vsf/`）

## 目录结构

```
├── docs/                 核心文档 + 10 个 ADR
├── vsf/                  VSF 上游源码（不直接改）
├── vsf-addon/            新增代码（MCU port / 组件）
├── wheels/               可复用轮子（L0 原语 → L3 模块）
│   ├── primitives/       L0：log, crc16, cli_parser, fal, kvs, mahony_ahrs, mag_cal
│   ├── drivers/          L1：w25qxx, qmi8658a, qmc5883p, xgzp6816d, gpio_led, gpio_button, ws2812
│   ├── protocols/        L2：at_command, modbus_rtu, ncp5623_parser, nmea_parser, ota_ab
│   ├── modules/          L3：（暂无）
│   └── _draft/           （暂无）
├── templates/            项目起步模板
├── projects/             真实项目（flybox, drone_light）
└── tooling/              patches / HIL / 脚本 / 实验
```

## 快速开始

```bash
# 跑全部 wheel 测试（Windows Git Bash 或 WSL）
bash tooling/scripts/test_all.sh --wheel

# 跑单个 wheel 测试
cd wheels/drivers/w25qxx/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure

# 审计所有 wheel 合规性
python tooling/scripts/audit_wheels.py

# 或用 Makefile 快捷命令
make test-wheels
make audit
```

## 新项目启动

```bash
mkdir -p projects/<name>
cp templates/project-integration/CLAUDE.md.template projects/<name>/CLAUDE.md
# 编辑 CLAUDE.md，然后参考 docs/integrating-wheels.md 接入 wheel
```

详细步骤见 `CLAUDE.md` 顶部的 7 步清单。

## 环境要求

- Windows 11 + WSL2 Ubuntu 24.04（或纯 Linux）
- WSL 内：gcc 13、cmake、ninja、pytest、pexpect
- QEMU（`qemu-system-arm`，用于 mps2-an500 bare-metal sim）

## 关键文档

| 文档 | 内容 |
|------|------|
| `CLAUDE.md` | AI 工作指令（零号铁律、commit 规范、任务流程） |
| `docs/philosophy.md` | 10/90 人-AI 分工愿景 |
| `docs/wheel-contract.md` | 轮子合格定义 + 升级流程 |
| `docs/api-conventions.md` | 跨 wheel API 命名/错误/线程/内存约定 |
| `docs/integrating-wheels.md` | 项目中接入 wheel 的 CMakeLists 示例 |
| `docs/ai-workflow.md` | 11 类任务标准路径 |
| `docs/debugging-playbook.md` | 踩坑指南 + mps2 checklist |
| `docs/adr/` | 10 个架构决策记录 |
| `wheels/README.md` | Wheel 完整目录 + registry 表格 |
