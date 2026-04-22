# 踩坑处理指南

遇到问题按以下顺序处理。AI 每解决一个非显然问题，回来更新此文档。

## 通用规则

1. **错误信息完整读一遍**——不要根据第一行猜。
2. **最小化复现**——删无关代码看问题还在吗。
3. **查本文档 + `memory/`**——大概率已遇过。
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
| 哈希 | `vsf_hash` | `service/hash/` |
| CRC | `vsf_hash/crc` | `component/hash/crc/` |
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

## 模板：新坑记录

```markdown
### [类别] 一句话描述

**错误**：（贴关键错误）

**原因**：（根因）

**修**：（命令 / 代码）

**发现于**：（项目/日期）
```

类别：`编译` / `链接` / `运行` / `测试` / `硬件` / `环境` / `工具链` / `VSF` / `NuttX` / `WSL`
