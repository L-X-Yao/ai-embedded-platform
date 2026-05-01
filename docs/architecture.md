# 技术架构：Wheel 被动库体系

## 设计哲学

Wheel 是**被动库**——被应用层调用，永远不主动做任何事。这是整个平台的第一性原则，所有其他规则从此推导。

### 三条铁律

1. **Wheel 不碰 OS** — 不创建任务、不注册中断、不 sleep、不分配堆内存、不加锁
2. **依赖只通过函数指针注入** — wheel 之间不 `#include` 彼此，硬件/OS 能力通过回调传入
3. **所有 wheel 必须 host 可测** — `gcc + cmake` 直接编译运行，无需 RTOS/硬件/模拟器

### 设计模式

依赖注入（Dependency Injection）+ 策略模式（Strategy Pattern），不是面向对象。

```c
// 接口定义：函数指针结构体
typedef struct {
    void (*cs_low)(void *ctx);
    void (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void *ctx;
} w25qxx_spi_t;

// wheel 只依赖接口，不知道谁实现了它
w25qxx_err_t w25qxx_read(const w25qxx_spi_t *spi, uint32_t addr, ...);
```

类比：Go interface / Rust trait / Linux 内核 `file_operations`。

## 分层架构

```
┌─────────────────────────────────────────────────┐
│  应用层（POSIX API — 平台规定）                   │
│  pthread_create / usleep / sem_wait / pipe       │
│  AI 最熟悉 + Linux 上直接仿真                     │
├─────────────────────────────────────────────────┤
│  Wheel（平台资产，跨项目复用）                     │
│  ┌─────────┬──────────┬──────────┬────────────┐ │
│  │ L3 模块 │ L2 协议  │ L1 驱动  │ L0 原语    │ │
│  │ 被动库  │ 编解码   │ HAL 注入 │ 纯算法     │ │
│  └─────────┴──────────┴──────────┴────────────┘ │
│  零 OS 依赖 / 零硬件依赖 / 纯 C99 / host 可测    │
├─────────────────────────────────────────────────┤
│  POSIX 实现层（项目选择）                         │
│  Linux 原生 / VSF 皮肤 / FreeRTOS-Plus-POSIX /   │
│  RT-Thread POSIX / Zephyr POSIX                  │
├─────────────────────────────────────────────────┤
│  硬件 HAL（厂商 SDK / VSF HAL / 直接寄存器）      │
└─────────────────────────────────────────────────┘
```

### 各层职责

| 层 | 做什么 | 怎么被调用 | 举例 |
|----|--------|-----------|------|
| L0 原语 | 纯计算/数据结构 | 直接函数调用 | crc16, state_machine, log |
| L1 驱动 | 通过 HAL 回调操作硬件 | 传入回调结构体 | bmp280, w25qxx |
| L2 协议 | 字节流 ↔ 结构体编解码 | 传入 buffer | mqtt_packet, modbus_rtu |
| L3 模块 | 组合 L0-L2 的逻辑 | feed/poll 模式 | cli_shell, data_logger |

### L3 设计要点

L3 和 L0-L2 同性质——**仍然是被动库**，不拥有任务：

```c
// cli_shell 不创建任务，不读 UART
// 应用层读 UART，喂给 shell，取输出
void cli_shell_feed(cli_shell_t *sh, uint8_t byte);
const char *cli_shell_poll(cli_shell_t *sh);
```

## 集成模型

Wheel 之间的连接发生在**应用层**，通过胶水代码：

```c
// 1. 硬件 HAL（连接 wheel → 芯片）
static uint8_t spi_xfer(void *ctx, uint8_t tx) {
    return gd32_spi_readwrite(SPI1, tx);
}

// 2. Wheel-to-wheel（一个 wheel 的能力喂给另一个）
static int kvs_read(uint32_t addr, uint8_t *buf, uint32_t len) {
    return w25qxx_read(&flash_spi, addr, buf, len);
}

// 3. 任务编排（POSIX/RTOS API 驱动 wheel）
void *sensor_task(void *arg) {
    while (1) {
        bmp280_read(&bmp, &temp, &press);
        usleep(100000);
    }
}
```

**深度整合**不是做不到，只是发生在应用层而非库层。链条（SPI → W25Qxx → FAL → KVS）和 RT-Thread 一样深，但每个连接是显式的。

AI 自动生成这些胶水代码是 EmbedForge 的核心价值。

## 切换 RTOS / MCU

| 切换什么 | 改什么 | Wheel 改动 |
|---------|--------|-----------|
| RTOS（VSF → FreeRTOS） | 应用层任务代码（pthread → xTaskCreate） | 零 |
| MCU（GD32 → STM32） | HAL 回调实现（gd32_spi → stm32_hal_spi） | 零 |
| 去掉 RTOS（→ 裸机） | 任务代码改为 main loop 轮询 | 零 |

Wheel 不知道自己运行在什么 OS 和什么硬件上。"跨平台"不是主动兼容的成本，而是不引入依赖的自然结果。

### 应用层统一 POSIX（平台规定）

应用层**一律使用 POSIX API**（pthread / usleep / sem_wait / pipe）。这是平台级决策，不是项目级选择。

**两个理由**：

1. **AI 效率最高**：POSIX 是 AI 训练数据中最密集的 OS API，AI 写 POSIX 代码正确率和速度高于任何 RTOS-specific API。
2. **全栈 host 仿真**：POSIX 应用逻辑在 Linux 上直接运行——pthread 是真线程，usleep 是真延时。不只是 wheel 能 host 测试，**整个应用都能 host 仿真**。

**"POSIX API" vs "POSIX 实现" 的区分**：

| | 范围 | 谁决定 |
|--|------|--------|
| POSIX API（应用层用什么函数） | pthread_create / usleep / sem_wait | **平台规定**，统一 |
| POSIX 实现（谁提供这些函数） | Linux 原生 / VSF 皮肤 / FreeRTOS-Plus-POSIX / RT-Thread POSIX 层 | **项目选择**，取决于 RTOS |

**新项目最高效开发流程**：

1. AI 用 POSIX 写全部应用逻辑
2. Wheel 用 mock HAL 测试
3. 应用逻辑在 Linux 上直接仿真
4. 全栈 host sim 验证通过
5. 接入目标 RTOS 的 POSIX 实现层
6. 烧录真机做最终 HIL 验证

前 4 步全在 host 上完成，不需要硬件。

**裸机例外**：资源极受限的项目（如 drone_light，30KB Flash）可能无法支撑 POSIX 层。这种情况降级为 main loop 轮询，但应视为例外而非常规。

## 与 RTOS 内置组件的关系

Wheel 和 RTOS 组件（如 RT-Thread 的 freemodbus、FAL、FinSH）**不冲突**：

- RTOS 组件 = 编解码 + 设备框架集成 + 任务管理（成品菜）
- Wheel = 编解码 / 算法 / 驱动核心（原料）

项目层面自由选择：用 RT-Thread 自带的 Modbus 还是我们的 modbus_rtu wheel。不重合的部分（如 mahony_ahrs、ota_ab）是独有价值。RTOS 越轻量（FreeRTOS/裸机），wheel 价值越大。

## Wheel 该做 vs 不该做

判断标准：**如果一个功能需要"自己创建任务"或"自己管理中断"，它不是 wheel。**

| 该做（被动库） | 不该做（OS 职责） |
|--------------|-----------------|
| json_parser（解析字节流） | memory_pool（内存管理） |
| base64（编解码） | thread_pool（任务管理） |
| ring_buffer（纯数据结构） | mqtt_client（需要任务循环） |
| cli_shell（feed/poll） | watchdog_mgr（需要中断） |
| data_logger（write/flush 回调） | power_mgr（需要 OS sleep） |

## 场景应对

### 项目级

| 场景 | 应对 |
|------|------|
| 新项目换 MCU | 只改 HAL 回调实现，wheel 零改动 |
| 新项目换 RTOS | 只改任务代码，wheel 零改动 |
| 项目需要线程安全 | 应用层调用 wheel 前加锁 |
| 项目需要 DMA | HAL 回调内部用 DMA，wheel 不感知 |
| 项目需要低功耗 | 应用层控制 sleep/wake，wheel 被动 |
| 中断驱动外设 | ISR 设 flag → 任务调用 wheel 处理 |

### 规模化

| 场景 | 应对 |
|------|------|
| Wheel 增长到 200+ | 扁平架构天然可扩展，每个 wheel 独立 |
| 多项目复用同一 wheel | 零成本——复制目录或 submodule |
| 需要跨 wheel 统一行为 | 通过 api-conventions.md 约束，不通过代码依赖 |

### 极端

| 场景 | 应对 |
|------|------|
| 安全关键（汽车/医疗） | 纯 C99 无动态内存无递归，易 MISRA 审计 |
| 资源极受限（8KB Flash） | 只选需要的 wheel，每个 ~1-2KB |
| VSF 上游停更 | wheel 不依赖 VSF → 零影响 |
| 多核 MCU | 每核独立 wheel 实例，核间通信是应用层的事 |
