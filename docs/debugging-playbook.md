# 踩坑处理指南

遇到问题按以下顺序处理。AI 每解决一个非显然问题，回来更新此文档。

## 通用规则

1. **错误信息完整读一遍**——不要根据第一行猜。
2. **最小化复现**——删无关代码看问题还在吗。
3. **查本文档 + Claude Code 自动记忆**——大概率已遇过。
4. **查 `vsf/example/template/demo/`**——VSF 知识多藏在示例里。
5. **只有以上都不行，才去猜 / grep 源码**。

---

## VSF 数据结构速查表（新建 wheel 前必查）

**AI 新建任何 wheel 前必须先看这张表**。VSF 原生已提供的，**不要重造**。

### 数据结构

| 你想要的 | VSF 已有 | 位置 | 用法 |
|---------|---------|------|------|
| 字节 ring buffer / 流 | `vsf_fifo_stream` | `service/simple_stream/fifo_stream/` | 带事件机制（connect TX/RX、on_evt） |
| 字节流 / mem stream | `vsf_mem_stream` | `service/simple_stream/mem_stream/` | 调用方提供 buffer |
| 类型化 FIFO（存结构体） | `vsf_fifo` | `service/fifo/` | `def_vsf_fifo(name, type, depth)` 模板式 |
| 单链表 | `vsf_slist_t` | `utilities/vsf_list.h` | `vsf_slist_append/remove` |
| 双链表 | `vsf_dlist_t` | `utilities/vsf_list.h` | `vsf_dlist_*` |
| 对象池 | `vsf_pool_t` | `service/pool/` | 预分配对象池 |
| 动态数组 | `vsf_dynarr_t` | `service/dynarr/` | 类 vector |
| 动态栈 | `vsf_dynstack_t` | `service/dynstack/` | LIFO |
| 堆 | `vsf_heap` | `service/heap/` | 可控内存池 |
| 优先队列（pbuf） | `vsf_pbuf_queue` | `service/stream/pbuf_queue/` | 数据包队列 |

### 同步原语

| 你想要的 | VSF 已有 | 用法 |
|---------|---------|------|
| 信号量 | `vsf_sem_t` + `vsf_eda_sem_*` | `vsf_eda_sem_init/post/pend` |
| 互斥锁 | `vsf_mutex_t` + `vsf_eda_mutex_*` | `vsf_eda_mutex_init/enter/leave` |
| 临界区 | `vsf_crit_t` + `vsf_eda_crit_*` | - |
| 触发器 / 事件 | `vsf_trig_t` + `vsf_eda_trig_*` | - |
| 消息队列 | `vsf_queue_t` | `vsf_eda_queue_*` |

**禁止**：新建"封装 mutex 的 wheel"或"ring buffer wheel"，VSF 已都有。

### 服务组件

| 你想要的 | VSF 已有 | 位置 |
|---------|---------|------|
| 日志 / trace | `vsf_trace` | `service/trace/` |
| JSON 解析 | `vsf_json` | `service/json/` |
| 哈希 | `vsf_hash` | `component/crypto/hash/` |
| CRC | `vsf_crc` | `component/crypto/hash/crc/` |
| ELF loader | `vsf_elfloader` | `service/loader/elf/` |
| 分布式总线 | `vsf_distbus` | `service/distbus/` |

### 内核原语（EDA）

| 任务形式 | API |
|---------|-----|
| 事件驱动任务 | `vsf_eda_start(eda, cfg)` + evthandler |
| 定时 EDA | `vsf_teda_start` + `vsf_teda_set_timer_ms` |
| PT 共享栈任务 | `vsf_pt_*`（类似 protothread） |
| 独立栈线程 | `vsf_thread_*`（依赖 setjmp） |
| FSM 状态机 | `vsf_fsm_*` |

### 皮肤层（Linux skin 下）

| 想要的 | 可用 |
|-------|------|
| pthread | `<pthread.h>` — VSF 皮肤完整支持 |
| socket | `<sys/socket.h>` — VSF 皮肤支持 |
| libusb | `<libusb.h>` — VSF 皮肤独有亮点 |
| 文件 I/O | `<unistd.h>` / `<fcntl.h>` 标准 POSIX |
| 49 个其他 POSIX headers | 见 `vsf/source/shell/sys/linux/include/` |

---

## GD32F4xx 调试快速包（HIL boot / fault / 栈溢出 一键定位）

> 📦 `tooling/scripts/gd32f4xx_debug/` —— flybox Phase B + 后期 triage 沉淀的 reusable 调试资产，**任何 GD32F4xx 项目可直接拿来用**（其中 5/7 是任意 Cortex-M / GCC ARM 项目通用）。

### 何时打开包

| 症状 | 工具 | 一行调用 |
|------|------|----------|
| Boot 卡死 / 静默 reset loop | `diag_halt.cfg` | `openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -f gd32f4xx_debug/diag_halt.cfg` |
| 想知道 MCU 卡在哪条指令 | `diag_where.cfg` | `openocd -f ... -f gd32f4xx_debug/diag_where.cfg` |
| HardFault / BusFault / UsageFault | `cortex_m4.gdbinit` → `fault_full` | GDB 中 `monitor halt; fault_full` |
| 怀疑栈溢出（HardFault on push） | `stack_analyzer.py` | `python stack_analyzer.py BUILD_DIR --task-config tasks.json` |
| 抓 boot burst 但 PowerShell 只抓两行 | `serial_capture.ps1` | `pwsh -File serial_capture.ps1 -Port COM25` |

### 快速包内容速查

- **`cortex_m4.gdbinit`**（任意 Cortex-M4）：`fault_info` / `exc_frame [sp]` / `ret_lr` / `fault_full` / `vtor`
- **`gd32f4xx.gdbinit`**（GD32F4xx）：`rcu_clk` / `i2c_regs <base>`
- **`diag_halt.cfg` / `diag_where.cfg`**（任意 Cortex-M）：OpenOCD 一次性 halt + 寄存器/PC 采样，免开 GDB 会话
- **`stack_analyzer.py`**（任意 GCC ARM）：从 `-fstack-usage` + objdump callgraph 静态分析 per-task 累积栈使用，配合 `--task-config tasks.json` 描述任务入口
- **`serial_capture.ps1`**（STM32 / GD32 + USB-UART）：CH340 DTR/RTS=true + 64KB ReadBuffer + 25ms 轮询，避免 boot burst 丢失

### 沉淀来源（每条工具背后的真实教训）

- `stack_analyzer.py` ← flybox mqtt_task_entry 累积 6KB > 4KB 分配 → HardFault on push（`feedback_debug_toolchain_first.md`）
- `cortex_m4.gdbinit::fault_full` ← 手解 Cortex-M4 fault 5 个寄存器组 + EXC_RETURN 4 位 + 栈帧 = 15 min/次，自动化后 < 1 sec
- `diag_halt.cfg` ← W25Q64 SPI 总线挂掉 spi_xfer 死循环 → reset loop。OpenOCD 一次性 halt + addr2line = 5 min（`feedback_hw_flag_wait_guard.md`）
- `serial_capture.ps1` DTR/RTS ← CH340 USB-UART 不拉高 DTR/RTS 会在第一波数据后停止转发 RX，让"诊断管道"撒谎 30 min（`feedback_verify_capture_pipeline.md`）

### 用法（项目级 gdbinit 套这个包）

参考 `tooling/scripts/flybox.gdbinit` 写法：

```gdb
source gd32f4xx_debug/cortex_m4.gdbinit
source gd32f4xx_debug/gd32f4xx.gdbinit

target extended-remote :3333
file build/myproject.elf
break HardFault_Handler
```

启动脚本（参考 `flybox_gdb.sh`）需 `cd` 到 `tooling/scripts/` 让相对 `source` 路径生效。

完整说明：`tooling/scripts/gd32f4xx_debug/README.md`。

---

## VSF mps2-an500 bare-metal sim 最小跑通配置（首选，canonical checklist）

> ✅ **首选 kernel-only sim 路径**（[ADR-008](adr/008-vsf-linux64-sim-strategy.md) 选项 C，2026-04-22 晚实证通过）。新项目跑 VSF kernel / shell / USB CDC 等 bare-metal 场景走这条，不走 linux64。

`tooling/experiments/vsf-tryouts/mps2_bare/` 是实证跑通的 scaffold——build + link + QEMU 调度 + semihosting 输出全通，外加 pytest harness 一把跑完 5 条断言。下面是 minimal 配置里的 7 个必踩坑。

### 工具链（WSL Ubuntu 24.04 apt）

```bash
sudo apt install -y qemu-system-arm gcc-arm-none-eabi \
                    binutils-arm-none-eabi libnewlib-arm-none-eabi
```

### 必须的 cmake 设置

```cmake
# arm-none-eabi-gcc 无法 link cmake 的 host compiler test，flags 必须先于 project() 设
set(CMAKE_C_FLAGS "-Ofast" CACHE INTERNAL "...")
include($ENV{VSF_PATH}/script/cmake/compilers/gnuarmemb.cmake)
project(vsf_mps2_bare)
set(VSF_TARGET an500)                   # Cortex-M7 / CMSDK_CM7 / mps2-an500

# linker script 复用 template/cmake/mps2/linker.ld（注释写 aic8800 但
# FLASH@0x0 / RAM@0x20000000 布局兼容 mps2-an500 memory map）
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    LINKER:-T$ENV{VSF_PATH}/example/template/project/cmake/mps2/linker.ld
    # cortex_m_generic.c 引用 _stext/_etext，aic8800 linker 只定义 __etext
    LINKER:--defsym=_stext=0
    LINKER:--defsym=_etext=__etext
)
```

### 必须的 vsf_usr_cfg.h

```c
#define VSF_KERNEL_CFG_ENTRY_IS_MAIN    ENABLED
#define VSF_SYSTIMER_FREQ               25000000  // cortex-m 必填，linux64 不需要
#define VSF_USE_TRACE                   ENABLED
#define VSF_USE_SIMPLE_STREAM           ENABLED   // trace 硬依赖
#define VSF_USE_LINUX                   DISABLED  // bare-metal 明确关
#define APP_USE_LINUX_DEMO              DISABLED
// 其他 APP_USE_* (LVGL/TGUI/SDL2/USBD/USBH/AUDIO/HAL/…) 全 DISABLED
```

### QEMU 启动

```bash
qemu-system-arm -M mps2-an500 -cpu cortex-m7 \
    -kernel build/vsf_mps2_bare.elf \
    -nographic -monitor null -semihosting
```

`-semihosting` 启用 ARM semihosting（`bkpt 0xAB`）——初期 probe 绕过 trace/UART 直接输出到 host stdio。

### 7 条必踩坑（按触发顺序）

1. **`VSF_SYSTIMER_FREQ` 缺失** → `vsf_arch_abstraction.c:583: error: 'VSF_SYSTIMER_FREQ' undeclared`。cortex-m 必填，linux64 target 不 check
2. **`VSF_USE_TRACE` 没配对 `VSF_USE_SIMPLE_STREAM`** → `vsf_trace.c:37: #error stream MUST be enabled to use trace`
3. **Linker 缺 `_stext/_etext`** → `cortex_m_generic.c:(.text.vsf_arch_low_level_init): undefined reference to '_etext'`。用 `--defsym` 补 alias，不碰 linker.ld（VSF upstream 文件）
4. **Template mps2 CMakeLists 强启 LVGL/DISP/FREETYPE** → 需 `git submodule update --init --recursive` 拉几百 MB 子模块。用 `mps2_bare` scaffold 外置配置避开（同昨天 linux64 kernel_test 策略）
5. **`vsf_trace_info` 静默失败**：没调 `vsf_trace_init(stream)` 时既不 crash 也没 output。Early probe 改用 ARM semihosting SYS_WRITE0 (0x04) 直接写字符串：

    ```c
    static void sh_write0(const char *s) {
        register const char *r1 asm("r1") = s;
        asm volatile ("mov r0, #0x04\n bkpt 0xAB" : : "r"(r1) : "r0", "memory");
    }
    ```

6. **VSF 自带 `kernel_test/sem_test.c` 等文件不能直接 drop-in**：它们在 `#if VSF_USE_TRACE == ENABLED` 里调 `vsf_start_trace()` **无参版本**，要求 `VSF_DEBUGGER_CFG_CONSOLE` / `VSF_HAL_USE_DEBUG_STREAM == ENABLED` / `VSF_CFG_DEBUG_STREAM_TX_T` 之一定义 + 对应 `VSF_DEBUG_STREAM_TX` 符号（BSP 级别 wiring）。没配齐会 compile error `vsf.h: expected expression before ')' token`。Minimal scaffold 短路方式：**放弃 `vsf_trace`，用 `printf` + stdio retarget**——override VSF weak `vsf_stdout_putchar` 到 ARM semihosting SYS_WRITEC (0x03)：

    ```c
    int vsf_stdout_putchar(char ch) {
        register char *r1 asm("r1") = &ch;
        asm volatile ("mov r0, #0x03\n bkpt 0xAB" : : "r"(r1) : "r0", "memory");
        return 0;
    }
    void vsf_stdout_init(void) {}
    void vsf_stdin_init(void) {}
    ```

    `mps2_bare/main.c` 用此法跑通 teda + sem ping-pong 测试（实证 VSF kernel 调度 + vsf_systimer + eda 同步）。

7. **QEMU `-nographic -semihosting` 把 `SYS_WRITEC` 送到 stderr，不是 stdout**（自动化场景踩的坑）：bash 手跑看不见这问题（终端把 stdout / stderr 都显示），但在 pytest / CI 用 `subprocess.run(capture_output=True)` 抓输出时，`r.stdout == ''`、rc == 0——以为 main.c 没打印，其实打了只是管道走 stderr。工作流：**fixture 读 `r.stderr`**（或 `stderr=subprocess.STDOUT` 合流）。验证环境 QEMU 8.2.2 / WSL Ubuntu 24.04。参见 `tooling/experiments/vsf-tryouts/mps2_bare/test_mps2_bare.py` fixture 里的注释。

### 预判外设可行性（proactive，上 sim 前先跑，2026-04-23 USB probe 发现）

**10 秒级外设可行性判定**——上 sim 前跑两条命令，避开几轮编译试错：

```bash
# QEMU 层：看 target 是否暴露目标外设
qemu-system-arm -M mps2-an500 -device help 2>&1 | grep -iE "usb|hid|eth|scsi"

# VSF 层：看 VSF 作者对此板能力的"真相表"
cat vsf/example/template/config/vsf_usr_cfg/vsf_usr_cfg_mps2.h  # 替换 board 名
```

**判读**：
- USB 相关条目都要求 `bus usb-bus` 或 `bus PCI`，mps2-an500 无 PCI bridge 也无 usb-bus 背板 → **USB device sim on mps2 直接证伪**
- `vsf_usr_cfg_mps2.h:91-92` `//#define VSF_USE_USB_DEVICE` 被注释 + 注明 "will be enabled if target chip supports USBD" → VSF 作者同判
- 同理可判 ethernet / LCD panel / SD host 等外设可行性

**相关**：ADR-008 § "USB device 在 mps2 不可达" / ADR-006 § Phase 0

---

## VSF linux64 target 最小跑通配置（canonical checklist）

> ⚠️ **读前必知**（**[ADR-008](adr/008-vsf-linux64-sim-strategy.md)**）：
> linux64 target 只适合 `APP_USE_LINUX_DEMO=ENABLED` busybox 模式。**本 checklist 是 kernel-only 的 tryout 脚手架——build + link 能通但 user_main 不会被调度**（根因见下方 "VSF linux64 kernel-only 模式 user_main 永不被调" 条目）。
> 
> **新项目要跑 kernel-only sim 请走 mps2+QEMU，不走这条**。本 checklist 保留价值在于：配置依赖链参考 + 排查基线 + 万一未来 VSF upstream 把 linux arch 补全时，本 scaffold 直接变 runnable。

跑通 VSF 在 host linux 的 sim 门槛一点都不低——下面是 2026-04 实测过的最小开关组合。**新项目起 linux sim 先照这张单子对，不要从 `example/template/project/cmake/linux/` 原地抄**（那是全家桶 GUI 模板，依赖未 checkout 的 LVGL/SDL2/FreeType submodule）。

**Canonical scaffold**（能 build + link 通，791K binary）：`tooling/experiments/vsf-tryouts/kernel_test/`

### 必须的 cmake 设置

```cmake
set(VSF_TARGET generic/linux64)        # 目标名叫 linux，不叫 sim
set(VSF_USE_LINUX 1)                    # cmake 层
set(VSF_LINUX_USE_SIMPLE_LIBC 1)        # cmake 层
set(VSF_CONFIG_PATH ${CMAKE_CURRENT_SOURCE_DIR}/config)
include($ENV{VSF_PATH}/script/cmake/vsf.cmake)
```

### 必须的 vsf_usr_cfg.h 开关

```c
// cmake `set(VSF_USE_LINUX 1)` 只告诉 cmake 编哪些源,
// 不会自动 #define 给 C 代码。显式 define 是必须的。
#define VSF_USE_LINUX                   ENABLED
#define VSF_USE_SIMPLE_STREAM           ENABLED

// 依赖链连锁（启一个就得把下面全启，否则一路 #error）
#define VSF_USE_FS                      ENABLED
#   define VSF_FS_USE_MEMFS             ENABLED
#define VSF_USE_MAL                     ENABLED
#define VSF_USE_TRACE                   ENABLED

// linux 不支持抢占,要 evtq 在 idle 里跑
#define VSF_OS_CFG_ADD_EVTQ_TO_IDLE     ENABLED
#define VSF_KERNEL_CFG_CPU_USAGE        DISABLED

// 让 VSF 占 main(),用户写 user_main()(= VSF_USER_ENTRY)。
// 不启它就 VSF_USER_ENTRY=main、OS 直接 call 用户 main、
// VSF 的 app_main_thread 永远没被驱动。
#define VSF_KERNEL_CFG_ENTRY_IS_MAIN    ENABLED

// simple libc
#define VSF_LINUX_USE_SIMPLE_LIBC       ENABLED
#if VSF_LINUX_USE_SIMPLE_LIBC == ENABLED
// 必须关——见下面"链接"类坑"__wrap_opendir 缺失"
#   define VSF_LINUX_CFG_WRAPPER        DISABLED
#   define VSF_USE_SIMPLE_SSCANF        ENABLED
#   define VSF_USE_SIMPLE_SPRINTF       ENABLED
#endif

// 所有 APP demo 全关——用户自写 VSF_USER_ENTRY
#define APP_USE_LINUX_DEMO              DISABLED
// ...（USBD/USBH/SCSI/TGUI/SDL2/KERNEL_TEST/JSON/CPP 全 DISABLED）
```

### 用户入口（main.c）

```c
#include "vsf.h"

int VSF_USER_ENTRY(void)   // 展开为 user_main (当 ENTRY_IS_MAIN 启时)
{
    // kernel 已由 constructor 启动,现在在 app_main_thread 的上下文里
    // TODO: call vsf_start_trace() / vsf_stdio_init() 等
    return 0;
}
```

### 不走官方 template 的理由

- `vsf/example/kernel_test/project/` 只有 `mdk`(Keil),没 linux 脚手架——误路 1
- `vsf/example/template/project/cmake/linux/CMakeLists.txt` 默认启 LVGL / SDL2 / FreeType / libjpeg / libpng / mbedtls / littlefs——要么 `git submodule update --init` 全 clone 几百 MB,要么 configure 就卡在 `raw/lvgl/` 空目录
- `vsf_usr_cfg_linux.h` 默认 `APP_USE_KERNEL_TEST=DISABLED` 且属于 vsf/ 树(ADR-005 禁改)——想改也改不得

**结论**：外置最小脚手架是唯一干净路径。

---

## VSF USB CDC sim 在 linux64 busybox 模式此路不通（2026-04-22 晚 sanity-check spike）

**结论**：VSF 上游在 linux64 host 上不启 USBD DCD 代码路径；`usbip_dcd` 也没有 linux-native BSD socket backend（只有 Windows WinSock + lwIP 两个 backend）。想在 linux64 host 做 USBD CDC sim 必须自港 `vsf_usbip_dcd_linux.c` + 解决 WSL2 宿主 `vhci_hcd` kernel 模块问题——多日工程，不是 sanity-check 范围。

### 3 条硬证据

- **E1**：`vsf/example/template/config/vsf_usr_cfg/vsf_usr_cfg_linux.h:43/125` 双开关 `APP_USE_USBD_DEMO` / `VSF_USBD_USE_DCD_USBIP` 均 `DISABLED`（VSF 上游自己都不测这条路）
- **E2**：`vsf/source/component/usb/driver/dcd/usbip_dcd/backend/` 只有两个 backend 实现——`vsf_usbip_dcd_win.c`（硬依赖 `#include <Windows.h>` + WinSock `SOCKET`）和 `vsf_usbip_dcd_lwip.c`（需 `VSF_USE_LWIP`）；**无** POSIX/BSD socket backend
- **E3**：linux 目标 CMakeLists（`vsf/example/template/project/cmake/linux/CMakeLists.txt`）默认拉 LVGL + SDL2 + FreeType + libpng/jpeg + zlib + MBEDTLS + LITTLEFS + LUA + COREMARK（几百 MB 子模块），但 `VSF_USE_USB_*` 一项都没配；busybox applets 里也无 usbip 观察面（`grep usbip vsf/source/shell/sys/linux` 零命中）

### 替代路径（真实项目触发时评估，当前不投）

- **路径 A — mps2 + lwIP + usbip_dcd over QEMU netdev**（最干净但多日工程）：复用已实证跑通的 mps2 scaffold（`tooling/experiments/vsf-tryouts/mps2_bare/`），加 CMSDK_ETHLAN eth driver + lwip 配置 + `usbip_dcd` LWIP backend；host 用 `usbip-utils` 吸附。三层（lwip / eth / usbip）都没踩过，是新 rabbit hole
- **路径 B — Windows host VSF（win target）**：`VSF_USBIP_DCD_CFG_BACKEND_WIN` 是 first-class path，Windows 原生有 WinSock + 装 `usbipd-win` 即可；但用户主开发环境是 WSL2，切 Windows native 成本高
- **路径 C — 真机 USB CDC**：任意 MCU target（GD32 / M484 / AT32 / MT071 / aic8800 等）USBD CDC demo 一键跑（VSF 主流做法）——真实项目触发时直接走这条，不做 sim

### 何时重新评估

- 真实项目要求在无硬件阶段就把 USB CDC 协议栈 sim 调通 → 届时走路径 A（或 B）
- VSF upstream 加了 `vsf_usbip_dcd_linux.c` / linux-native socket backend → 本结论失效

**相关**：ADR-008 § 选项 B（同一"VSF 在 linux64 host 上某些 sim 路径此路不通"家族的第二个案例——第一个是 ADR-008 主体的 kernel-only 路径证伪）

---

## 已收集的坑（项目遇到后追加）

### [编译] VSF cmake 报 `plooc.h: No such file`

**原因**：PLOOC 子模块没 clone。

**修**：
```bash
cd vsf/source/utilities/3rd-party/PLOOC
git clone --depth 1 https://github.com/GorgonMeducer/PLOOC.git raw
```

---

### [编译] VSF cmake 报 `vsf_usr_cfg.h: No such file`（host lib 编译时）

**原因**：VSF 为 host system 创建独立静态库（`vsf_host_linux`），它不认项目 include path。

**修**：CMakeLists.txt 在 `include(vsf.cmake)` **之前**加：
```cmake
set(VSF_CONFIG_PATH ${CMAKE_CURRENT_SOURCE_DIR})
```

---

### [编译] `#error stream MUST be enabled to use trace`

**原因**：`VSF_USE_TRACE` 依赖 `VSF_USE_SIMPLE_STREAM`。

**修**：`vsf_usr_cfg.h` 同时开：
```c
#define VSF_USE_TRACE            ENABLED
#define VSF_USE_SIMPLE_STREAM    ENABLED
```

---

### [编译] `#error VSF_USE_MAL MUST be enabled to support mount`

**修**：`#define VSF_USE_MAL  ENABLED`

---

### [编译] `undefined reference to __vsf_linux_ctype`（链接时）

**错误**：
```
undefined reference to `__vsf_linux_ctype'
undefined reference to `__vsf_linux_errno'
```

**原因**：**cmake 变量 ≠ C 宏**。`set(VSF_USE_LINUX 1)` 只告诉 cmake 把 `source/shell/sys/linux/` 目录编进来,但 VSF 的 C 源码里 `vsf_linux_glibc_ctype.c:22` 是:
```c
#if VSF_USE_LINUX == ENABLED && VSF_LINUX_USE_SIMPLE_LIBC == ENABLED && VSF_LINUX_USE_SIMPLE_CTYPE == ENABLED
```
没 `#define VSF_USE_LINUX ENABLED` 这个宏,整个文件被 `#if` 出去——**编译器编了个空 object**,link 时符号缺失。

**修**：在 `vsf_usr_cfg.h` 显式写 `#define VSF_USE_LINUX ENABLED`。所有以 cmake var 形式设的 VSF_USE_* 都要镜像一份到 `vsf_usr_cfg.h`。

**发现于**：2026-04 VSF linux sim 初次搭脚手架

---

### [链接] `undefined reference to __wrap_opendir / __wrap_stat / __wrap_readdir_r`

**错误**：
```
undefined reference to `__wrap_opendir'
undefined reference to `__wrap_closedir'
undefined reference to `__wrap_stat'
```

**原因**：VSF 的 linker wrap 机制**两半脱节**:
- Header 半：`#define opendir VSF_LINUX_WRAPPER(opendir)` → `__wrap_opendir`(受 `VSF_LINUX_CFG_WRAPPER` 门控)
- Linker 半：`-Wl,--wrap=opendir` 由 `compilers.cmake` 的 `foreach(api ${VSF_TARGET_WRAPPER_LIST})` 循环添加

`linux64.cmake` 里 `VSF_TARGET_WRAPPER_LIST` **从未被设置**(整个仓库没 producer),所以 linker flag 永远不加——但 header 还是把 opendir rewrite 到 `__wrap_opendir`,于是找不到实现。

**修**：**关 wrapper**(简单且对大部分场景够用):
```c
#define VSF_LINUX_CFG_WRAPPER           DISABLED
```
代价:VSF 的 fakefs / mount 路径被旁路,`opendir` 直接链 glibc。kernel-only 或纯计算 demo 无感;要用 VSF fakefs 才有影响。

**根治**(未做):自己在 project CMake 里填充 `VSF_TARGET_WRAPPER_LIST`——工程量大,等真实项目要 fakefs 再做。可能也值得给 VSF upstream 提 patch。

**发现于**:2026-04 VSF linux sim 初次搭脚手架

---

### [运行] VSF 的 main 启动了但 user_main 没被调、无输出

**错误**:进程 exit 0,stdout 空。

**原因**:`VSF_KERNEL_CFG_ENTRY_IS_MAIN` 没启。
- 启它:VSF 提供 `int main(void)`,在 kernel thread 里 call 用户的 `user_main()`(= `VSF_USER_ENTRY`)。
- 不启它:`VSF_USER_ENTRY` 展开为 `main`——用户的 main 成了 OS entry,`app_main_thread_t` 创建后从没被驱动,process 跑完用户 main 就 exit(即使调 `vsf_delay_ms` 也不 yield)。

**修**:
```c
#define VSF_KERNEL_CFG_ENTRY_IS_MAIN    ENABLED
```
并用 `int VSF_USER_ENTRY(void)` 而非 `int main(void)` 作用户入口函数名。

**发现于**:2026-04 VSF linux sim 初次搭脚手架

---

### [运行] VSF linux64 kernel-only 模式 user_main 永不被调(根因已定位)

**现象**：binary build+link 通,进程启动,kernel `main()` 执行,但 `user_main()`(`VSF_USER_ENTRY`) 从未被调——进程进 kernel `while(1)` evtq 循环后不响应任何调度。stdout 也就没输出。

**误导**：表现像"stdio 没 flush / 没连 tty",实际是 **user_main 的 schedule path 从未 wire 起来**——kernel 没调度到它,自然也没 printf。

**根因诊断路径(2026-04 实证)**:
1. **constructor 能跑** → 用 `__attribute__((constructor(101)))` + 直接 inline asm syscall `exit(101)` → 进程 exit 101 ✓。证明 binary 正常 start。
2. **VSF main() 能跑** → override WEAK `vsf_main_entry`,syscall exit(55) → 进程 exit 55 ✓。证明 `main()` → `vsf_main_entry()` 链路 OK。
3. **user_main 永不被调** → user_main 第一行 syscall exit(42) → 进程超时(exit 124) ✗。证明 `__post_vsf_kernel_init` 里 `init_vsf_thread(app_main_thread_t)` **推入 evtq 的 INIT 事件从未被 `vsf_main_entry` 的 `while(1)` 循环 dispatch**。

**尝试过失败的解法**:
- 关 `VSF_KERNEL_CFG_SUPPORT_THREAD`(想走 IDLE 模式) → VSF linux 子系统硬依赖 `vsf_thread_t` 类型(`vsf_linux.h:119/181/182`),编译失败
- 启 `VSF_ARCH_ENTRY_NO_PENDING`(想走 host_thread_init 主线程分支) → 需要 `vsf_arch_main` 符号,linux arch 没实现,link 失败

**暂时结论**:
- VSF linux64 target **不是一个现成可直接跑 kernel-only 示例的 target**
- 官方 linux 路径都走 `APP_USE_LINUX_DEMO=ENABLED` + busybox shell 模式——user_main 通过 `vsf_linux_init(&stream)` 在里面特殊机制被驱动,**不走正常的 kernel app_main_thread**
- 直接 kernel-only(关 `APP_USE_LINUX_DEMO`)看起来像 VSF 的 unfinished path 或需要 upstream patch

**干净路径选项**(未来触发时选):
- **A**:接受 linux target = busybox-only,要跑 kernel_test 就启全部 `APP_USE_LINUX_DEMO` 依赖链(需 `git submodule update --init` 全 clone LVGL/SDL2/...)
- **B**:给 VSF upstream 提 patch,让 linux arch 实现 `vsf_arch_main`+启 `ENTRY_NO_PENDING` 支持 kernel-only
- **C**:用 QEMU 跑 mps2 target(arm),kernel-only 在 arm bare-metal 下是 first-class 支持

**canonical scaffold**:`tooling/experiments/vsf-tryouts/kernel_test/` —— build+link 通的最小配置(不跑)。作为排查基线。

未来解决了更新此节。

---

### [NuttX] `Wstack-usage=` 缺值

**修**：`echo 'CONFIG_STACK_USAGE_WARNING=0' >> .config && make olddefconfig`

---

### [NuttX] `CONFIG_NFILE_DESCRIPTORS_PER_BLOCK` undeclared

**修**：`echo 'CONFIG_NFILE_DESCRIPTORS_PER_BLOCK=8' >> .config && make olddefconfig`

---

### [NuttX] `nuttx/config.h: No such file`

**原因**：没跑 `make context`。

**修**：`make context && make -j$(nproc)`

---

### [NuttX] 加新 app 后 nsh 报 `command not found`

**原因**：`apps/examples/Kconfig` 是 mkkconfig.sh 生成的，不自动包含新目录。

**修**：
```bash
cd apps/examples && bash ../tools/mkkconfig.sh -o Kconfig
cd <nuttx>
echo 'CONFIG_EXAMPLES_<NAME>=y' >> .config
make olddefconfig && make context && make -j$(nproc)
```

---

### [WSL] clone 后 shell 脚本报 `/usr/bin/env: 'bash\r'`

**原因**：Windows Git 加了 CRLF。

**修**：在 WSL 里重新 clone（Linux git 不做 CRLF 转换）。

---

### [VSF] MinGW / TDM-GCC 在 Windows native 编译失败

**原因**：VSF `x86/win` target 假设 MSVC，MinGW 下 `_alloca / clockid_t / off_t` 等冲突。

**修**：**不推荐** MinGW 路径。Windows 下要么用 MSBuild（需 VS Build Tools），要么用 WSL。见 ADR 004。

---

### [VSF] linux shell 初始化：pthread_create 直接挂死

**错误**：调用 `pthread_create` 无任何输出，程序挂死。

**原因**：未调用 `vsf_linux_init()` 初始化 linux shell 的进程模型。pthread 依赖 linux 进程上下文（PID、线程列表、fd 表），裸调必挂。

**修**：在 `VSF_USER_ENTRY` 中先调 `vsf_linux_init(&stream)`，提供 stdio 流。应用代码必须作为 linux 进程运行（通过 `busybox_bind(VSF_LINUX_CFG_BIN_PATH "/init", app_main)` 注册）。

**发现于**：flybox 迁移预研 / 2026-04-24

---

### [VSF] linux shell：vsf_linux_init 返回但 init 进程不启动

**错误**：`vsf_linux_init` 返回 0，但注册的 init 进程永远不执行。

**原因**：`VSF_USER_ENTRY` return 后主线程结束，idle 循环不处理事件队列。linux 内核进程无法被调度。

**修**：主线程不要 return，改为 `while(1) { vsf_thread_delay_ms(1000); }` 持续让出 CPU。

**发现于**：flybox 迁移预研 / 2026-04-24

---

### [VSF] linux shell：init 路径必须用 VSF_LINUX_CFG_BIN_PATH

**错误**：`busybox_bind("/sbin/init", ...)` 后 init 不启动，打印 "linux: missing init?"。

**原因**：VSF 内核进程调 `execlp(VSF_LINUX_CFG_BIN_PATH "/init", ...)` 查找 init。默认 `VSF_LINUX_CFG_BIN_PATH = "/bin"`，所以路径是 `/bin/init`，不是 `/sbin/init`。

**修**：`busybox_bind(VSF_LINUX_CFG_BIN_PATH "/init", app_main)`。

**发现于**：flybox 迁移预研 / 2026-04-24

---

### [VSF] linux shell：busybox_install() 在 mps2-an500 上挂死

**错误**：`busybox_install()` 调用后不返回，程序停在此处。

**原因**：未排查根因（可能依赖终端或 FS 子系统）。flybox 不需要 busybox shell。

**修**：跳过 `busybox_install()`，直接 `busybox_bind()` 注册 init 入口。

**发现于**：flybox 迁移预研 / 2026-04-24

---

### [VSF] linux shell：VSF_USE_FS + VSF_USE_MAL 是硬依赖

**错误**：`#error VSF_USE_FS MUST be enabled to use fs in linux`。

**原因**：linux shell 内部依赖 VFS（fd 表、mount 等）。VFS 又依赖 MAL（block device 抽象）。

**修**：`vsf_usr_cfg.h` 加 `#define VSF_USE_FS ENABLED` + `#define VSF_USE_MAL ENABLED`。

**发现于**：flybox 迁移预研 / 2026-04-24

---

### [VSF] linux shell：newlib 类型冲突（clock_t / off_t）

**错误**：`error: unknown type name 'clock_t'` 或 `conflicting types for 'off_t'`。

**原因**：arm-none-eabi-gcc 的 newlib 定义了 `clock_t`/`off_t`，VSF linux shell 也定义了，两者冲突。

**修**：启用 VSF 自带的 simple_libc 替代 newlib 的冲突部分：
```c
#define VSF_LINUX_USE_SIMPLE_LIBC    ENABLED
#define VSF_LINUX_CFG_RELATIVE_PATH  ENABLED
```
CMake 中加 `set(VSF_LINUX_USE_SIMPLE_LIBC 1)`。

**发现于**：flybox 迁移预研 / 2026-04-24

---

## 模板：新坑记录

```markdown
### [类别] 一句话描述

**错误**：（贴关键错误）

**原因**：（根因）

**修**：（命令 / 代码）

**发现于**：（项目/日期）
```

类别：`编译` / `链接` / `运行` / `测试` / `硬件` / `环境` / `工具链` / `VSF` / `NuttX` / `WSL`
