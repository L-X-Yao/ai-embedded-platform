# ai-embedded-platform

[![CI](https://github.com/L-X-Yao/ai-embedded-platform/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/L-X-Yao/ai-embedded-platform/actions/workflows/ci.yml)

AI 驱动的嵌入式开发母仓库。人负责需求、架构决策和终审，AI 负责实现、测试、文档和工具链维护。

## 核心理念

所有 wheel（可复用组件）遵循三条铁律：

1. **不碰 OS** — 不创建任务、不加锁、不 sleep、不分配堆内存
2. **依赖注入** — 硬件/OS 能力通过函数指针传入，wheel 之间零耦合
3. **Host 可测** — `gcc + cmake` 直接编译运行，无需 RTOS 或硬件

结果：每个 wheel 在任意 MCU / 任意 RTOS / 纯 PC 上都能跑。AI 写一次，处处复用。52 项自动审计保证一致性。

## 平台现状

- **186 个 candidate wheels**（L0 原语 61 / L1 驱动 74 / L2 协议 31 / L3 模块 20）
- **3 个集成示例**：env_monitor / device_shell / remote_telemetry
- **2 个真实项目**：flybox（无人机遥测盒子，GD32F405）/ drone_light（LED 灯控，N32G003）
- **CI 增量检测** + 52 项 wheel 审计全绿
- **基座**：[VSF](https://github.com/vsfteam/vsf)（永久基座，vendored 在 `vsf/`）

## 目录结构

```
├── docs/                 核心文档 + 11 个 ADR
├── vsf/                  VSF 上游源码（不直接改）
├── vsf-addon/            新增代码（MCU port / 组件）
├── wheels/               可复用轮子（L0 原语 → L3 模块）
│   ├── primitives/       L0 (61)：log 生态, crc 族, 数据结构, 编解码, 数学, 内存, 字体, ...
│   ├── drivers/          L1 (74)：传感器, IMU, 显示, 存储, 通信, RFID, RTC, IO 扩展, ...
│   ├── protocols/        L2 (31)：mqtt, modbus, coap, http, lorawan, canbus, dhcp, tftp, ...
│   ├── modules/          L3 (20)：cli_shell, data_logger, ota_manager, ...
│   └── _draft/           （暂无）
├── templates/            项目起步模板
├── projects/             真实项目（flybox, drone_light）
├── examples/             集成示例（wheel 组合食谱）
└── tooling/              harness / codegen / test / audit / wheel / session / ci / debug / metrics / hil
```

## 快速开始

```bash
# 浏览可用 wheel
cat wheels/README.md            # 完整目录 + 分类
ls wheels/drivers/              # 按层级浏览

# 在项目中使用 wheel（CMakeLists.txt 加一行）
add_subdirectory(${WHEELS_DIR}/drivers/bmp280 bmp280)
target_link_libraries(my_app PRIVATE bmp280)

# 跑测试
make test-wheels                # 全部 wheel 测试
make test-changed               # 仅受改动影响的测试

# 跑审计
make audit                      # 52 项合规检查
python tooling/audit/audit_wheels.py --wheel drivers/bmp280  # 单个 wheel
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
| `docs/philosophy.md` | 人-AI 分工愿景 |
| `docs/wheel-contract.md` | 轮子合格定义 + 升级流程 |
| `docs/api-conventions.md` | 跨 wheel API 命名/错误/线程/内存约定 |
| `docs/integrating-wheels.md` | 项目中接入 wheel 的 CMakeLists 示例 |
| `docs/ai-workflow.md` | 12 类任务标准路径 |
| `docs/debugging-playbook.md` | 踩坑指南 + mps2 checklist |
| `docs/adr/` | 11 个架构决策记录 |
| `wheels/README.md` | Wheel 完整目录 + registry 表格 |
