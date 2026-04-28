# GD32F4XX VSF HAL Port

GD32F4xx (Cortex-M4, GigaDevice) 的 VSF HAL 驱动 port。

## Status

GPIO/USART/Flash/SPI/I2C 五个外设驱动已通过 flybox HIL 验证（600s soak, 0 watchdog reset, 2026-04-26）。CI cross-compile 全绿。代码稳定但未经第二个 GD32F4 项目复验，整体标记 **candidate**。

## 支持的型号

- GD32F405 ✅ (device.h 已写)
- GD32F407 / GD32F425 / GD32F427 / GD32F450 / GD32F470（__device.h 已预留路由，device.h 待需时创建）

## 已实现的外设

| 外设 | 状态 | 来源 | 说明 |
|------|------|------|------|
| GPIO | HIL verified | 复用 GD32H7XX | 寄存器布局完全一致 |
| USART | HIL verified | 改写自 GD32H7XX | F4XX 无 FIFO/CMD/INTC，用 STAT0+DATA 清中断 |
| Flash (FMC) | HIL verified | 改写自 GD32H7XX | 非均匀扇区(16K×4+64K+128K×7)，word 编程 |
| SPI | HIL verified | 从零编写 | Master/Slave 模式，8/16-bit，FIFO 轮询+中断 |
| I2C | HIL verified | 从零编写 | Master 事件驱动状态机(EV5/6/7/8)，EV+ER 双中断 |

## Why not VSF native?

VSF 上游只有 GD32VF103 (RISC-V) 和 GD32H7XX (Cortex-M7) 的 port。
GD32F4xx 是 Cortex-M4 系列，寄存器布局与 GD32H7XX 高度相似（GPIO 完全一致），
但 RCU 时钟树、中断号、Flash 分区不同，无法直接复用。

## 目录结构

```
GD32F4XX/
├── __device.h              系列级设备路由
├── GD32F405/
│   └── device.h            GD32F405 专有配置（GPIO/USART/Flash 定义）
├── common/
│   ├── common.h            RCU 时钟/复位 region 枚举
│   ├── driver.h            VSF 模板实例化入口
│   ├── gpio/               GPIO 驱动（复用 GD32H7XX 逻辑）
│   ├── usart/              USART 驱动（改写自 GD32H7XX，适配 F4XX 寄存器）
│   ├── flash/              Flash (FMC) 驱动（改写自 GD32H7XX，非均匀扇区）
│   ├── spi/                SPI 驱动（从零编写，master + FIFO 轮询）
│   ├── i2c/                I2C 驱动（从零编写，master 事件状态机）
│   └── vendor/             GD32F4xx 官方外设库头文件（需手动放置）
└── README.md               本文件
```

## Vendor 库集成

本 port 需要 GD32F4xx 官方外设库的头文件。将以下文件放入 `common/vendor/`：

```
common/vendor/
├── Include/
│   ├── gd32f4xx.h                 设备总定义（含 CMSIS）
│   ├── gd32f4xx_gpio.h            GPIO 寄存器宏
│   ├── gd32f4xx_rcu.h             RCU 寄存器宏
│   ├── gd32f4xx_usart.h           USART 寄存器宏
│   └── ...（按需添加）
└── Source/
    └── system_gd32f4xx.c          系统初始化（可选）
```

当前 vendor 库来源：`projects/flybox/third_party/`
- CMSIS 头文件 → `projects/flybox/third_party/CMSIS/`
- 外设库 → `projects/flybox/third_party/gd32_periph/`

## 使用方式

在项目的 `vsf_usr_cfg.h` 或 CMake 中定义：

```c
#define __GigaDevice__
#define __GD32F405__
#define GD32F405
#define VSF_DRIVER_HEADER   "vsf-addon/hal/driver/gd/GD32F4XX/common/driver.h"
```

`VSF_DRIVER_HEADER` 跳过 VSF 内置路由，直接指向本 port。无需 patch vsf/。

## 验证平台

- GD32F405RGT6 (Flybox 项目硬件)
- CI cross-compile: gcc-arm-none-eabi 13.x (`.github/workflows/ci.yml` flybox-build job)
- HIL 验证: flybox 固件在 GD32F405 + Air780EG 上跑 600s soak，0 watchdog reset (2026-04-26)
- 已验证外设链路: SPI1 (W25Q64), I2C1 (QMI8658A), 软件 I2C (XGZP6816D, QMC5883P), USART0/1, GPIO, FMC
