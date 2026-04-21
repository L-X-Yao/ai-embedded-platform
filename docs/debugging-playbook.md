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

### [运行] VSF 二进制启动后无任何输出（linux skin + host 场景）

**现状**：目前没完全解决。已知：

- 需 `VSF_HAL_USE_DEBUG_STREAM = ENABLED`
- main 里调用顺序：`VSF_STREAM_INIT` → `vsf_driver_init()` → `vsf_start_trace()` → `vsf_stdio_init()` → `vsf_linux_init(&stream)`
- 仍有未定位问题——**建议对照 `vsf/example/template/demo/linux_demo/linux_demo.c` 逐字**

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
