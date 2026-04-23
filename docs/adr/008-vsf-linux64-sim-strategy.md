# ADR 008: VSF linux64 target 不是 kernel-only sim 的可行路径

**状态**：accepted
**日期**：2026-04-22
**决策人**：@user

## 背景

2026-04-22 做 SIM 投资期探索（目标：趟通 VSF 在 host sim 上跑 kernel_test/shell/USB CDC），在 `vsf/example/template/project/cmake/linux/` 基础上外置最小脚手架 `tooling/experiments/vsf-tryouts/kernel_test/`。

**做得到**：build + link 通过（791K binary），kernel `main()` → `vsf_main_entry()` 链路 OK。

**做不到**：`user_main()`（`VSF_USER_ENTRY`）从未被调度执行。3 步 inline asm syscall probe 实证:
- constructor 直接 syscall exit(101) → 进程 exit 101 ✓（binary 正常启动）
- override WEAK `vsf_main_entry` syscall exit(55) → 进程 exit 55 ✓（VSF main 链路 OK）
- user_main 第一行 syscall exit(42) → 进程 timeout（124）✗（user_main 永不被调）

根因：linux64 target **不启** `VSF_ARCH_ENTRY_NO_PENDING`，`vsf_main_entry` 走 `while(1)` evtq 循环分支，`__post_vsf_kernel_init` 里 `init_vsf_thread(app_main_thread_t)` 推进 evtq 的 INIT 事件**从未被 dispatch**（或 dispatch 了但 host_thread pthread_create 链路不工作）。

**尝试过的两条出路都失败**:
- 关 `VSF_KERNEL_CFG_SUPPORT_THREAD`（走 IDLE 模式）→ `vsf_linux.h:119/181/182` 硬依赖 `vsf_thread_t`，编译失败
- 启 `VSF_ARCH_ENTRY_NO_PENDING`（强制 host_thread_init 主线程）→ 需要 `vsf_arch_main` 符号，linux arch 未实现，link 失败

VSF 官方 linux 示例（`linux_demo.c`）能跑是因为它走 `APP_USE_LINUX_DEMO=ENABLED` + busybox 模式——user_main 通过 `vsf_linux_init(&stream)` 的特殊机制被驱动，**不走**普通的 kernel `app_main_thread` 调度。

## 考虑过的选项

### A. 继续深挖 linux64 kernel-only 直到跑通

- **做法**：要么给 VSF upstream 提 patch 实现 linux `vsf_arch_main` + 启 `ENTRY_NO_PENDING`；要么自己在 project 里实现 `vsf_arch_main` + 补 WRAPPER_LIST
- **优点**：linux host 开发最顺手（不需装 QEMU），真跑通后所有 kernel case 在 linux 下都能搞
- **缺点**：工程量大且不确定（VSF upstream 这块看起来是 unfinished）；今天已经投资 2+ 小时没出结果
- **ROI 判断**：低。对未来项目的直接映射弱（多数真实项目是 MCU 不是 linux）

### B. 启全量 `APP_USE_LINUX_DEMO` busybox 模式

- **做法**：`git submodule update --init --recursive` 拉全部 VSF 3rd-party 子模块（LVGL / SDL2 / FreeType / mbedtls / littlefs 等），启 `APP_USE_LINUX_DEMO` + 所有依赖链，跑 busybox shell 里的 `eda_test` applet
- **优点**：这是 VSF 官方测试过的路径，能真跑
- **缺点**：
  - 拉 ~几百 MB 外部依赖；build 时间长
  - 跑起来只是"busybox 里跑 kernel_test applet"——不是"我们掌控 VSF kernel 启动"；对真实 MCU 项目的映射不直接
  - （2026-04-22 晚 usbip sanity-check spike 补）busybox 模式下"做 USBD CDC sim"这条很常见的需求 **linux64 host 上跑不了**——VSF `usbip_dcd` 只有 Windows（硬依赖 `#include <Windows.h>` + WinSock）和 lwIP 两个 backend，**没有 linux-native BSD socket backend**；`vsf_usr_cfg_linux.h:43/125` 双开关 `APP_USE_USBD_DEMO` / `VSF_USBD_USE_DCD_USBIP` VSF 上游自己都 `DISABLED`。想上必须自港 `vsf_usbip_dcd_linux.c` + WSL2 装 `vhci_hcd` kernel module + `usbip-utils`，多日工程。详见 `docs/debugging-playbook.md` § "VSF USB CDC sim 在 linux64 busybox 模式此路不通"
- **ROI 判断**：中低。能跑起来但学到的是 busybox 用法，不是 VSF kernel；且"busybox 能干的典型 sim 场景"里 USB 这条还进一步踩空

### C. 切 mps2 + QEMU 作为 kernel-only sim 目标 ✓

- **做法**：VSF 已有 `vsf/example/template/project/cmake/mps2/` target（`VSF_TARGET=an500`，Cortex-M7 / `mps2-an500`，`.vscode/launch.json` 已配 QEMU servertype）；kernel_test 的独立 example（`example/kernel_test/project/mdk/`）就是 bare-metal 模式
- **优点**：
  - bare-metal target 是 VSF first-class 支持（`VSF_ARCH_ENTRY_NO_PENDING` 等机制都为 MCU 设计）
  - 对真实项目映射直接（真实项目都是 MCU）
  - QEMU 跑 Cortex-M3 很轻（几百 KB），CI 友好
  - 能真正测 kernel 调度、中断、systimer 等（linux host 上这些都被 pthread 模拟）
- **缺点**（2026-04-22 晚 feasibility sanity check 补）：
  - 工具链 setup：`qemu-system-arm` + `gcc-arm-none-eabi` 默认都没装，WSL apt install 需 sudo（各 ~30 秒）
  - template CMakeLists 强启 `VSF_USE_LVGL/DISP/FREETYPE`——下次真实触发时要么 `git submodule update --init` 拉 LVGL/FreeType/evm 子模块，要么复用昨天 linux64 minimal scaffold 思路外置一份最小配置绕开 GUI 依赖
  - 调试反馈没 linux native 快；stdout 要经 QEMU serial 转发
- **ROI 判断**：高

### D. `fake_chip` target

- **做法**：`vsf/example/template/project/cmake/fake_chip/` 是"最简 arch 模拟"，linker.ld 用的 stub
- **优点**：比 mps2 更轻
- **缺点**：不清楚状态（fake 意味着很多 HAL 原语是 stub）；目的似乎是 arch porting 测试而非应用 sim
- **ROI 判断**：低（偏研究，不适合做"提前踩坑"目的）

## 决策

**选 C：kernel-only SIM 走 mps2 + QEMU。linux64 target 定性为 "仅用于 busybox 模式 Linux 用户空间 demo"——不投资把它变成 kernel-only sim。**

具体：
- 真实项目触发需要 sim 时，**首选** mps2 + QEMU（除非项目本身就要跑 linux userspace，那时再启 busybox 模式）
- `tooling/experiments/vsf-tryouts/kernel_test/` 脚手架**保留**作为配置依赖链参考基线，README 和 playbook 都明确标注 "build 通但 user_main 不跑"
- **不主动**再投资 linux64 kernel-only——除非真实项目触发且业务理由强

## 后果

### 好

- 选 sim target 时有明确指南，不会再走 linux64 kernel-only 弯路
- mps2+QEMU 对真实 MCU 项目映射直接——今天没做的 kernel_test/shell/USB CDC 跑通后的知识能立即套用
- 封装 2026-04-22 的 negative finding，未来 AI/人不用从头重新验证

### 坏

- ~~mps2+QEMU 自身没跑通实证~~ **2026-04-22 晚完整实证通过**：minimal scaffold `tooling/experiments/vsf-tryouts/mps2_bare/` build+link 通（32K text / 128K heap），QEMU `-M mps2-an500 -cpu cortex-m7 -semihosting` 启动后 user_main **被正常调度执行**（ARM semihosting `bkpt 0xAB` probe 验证输出 `[user_main] reached` + exit 42）。和 linux64 "user_main 永不被调" 形成鲜明对比——选项 C 从"选了但没验证"升级为"实证 first-class 路径"
- 如果未来项目要求 linux host sim（例如 VSF 工具链跑在 linux server 上提供 web 服务），这条 ADR 需要重评
- `tooling/experiments/vsf-tryouts/kernel_test/` 留着但不活跃——可能 VSF upgrade 后 build 不通也没人注意到

## USB device 在 mps2 不可达（2026-04-23 USB probe 补）

选 C（mps2+QEMU）的**边界补强**：kernel / teda / sem / systimer / log 已全部跑通，但 **USB device stack 在 mps2-an500 target 下硬性不可达**。

### 3 条决定性证据

1. **QEMU 硬件层**：`qemu-system-arm -M mps2-an500 -device help` 所有 USB 条目都要 `bus usb-bus` 或 `bus PCI`——mps2-an500 物理上无 PCI bridge 也无 usb-bus 背板
2. **VSF HAL 层**：`grep -i usb vsf/source/hal/driver/arm/mps2/` **零命中**——mps2 port 未实现任何 USB peripheral
3. **VSF 官方 config 层**：`vsf/example/template/config/vsf_usr_cfg/vsf_usr_cfg_mps2.h:91-92` `//#define VSF_USE_USB_DEVICE` 被 VSF 作者注释，明文 "will be enabled if target chip supports USBD"

三层一致认定——mps2 物理上无 USB IP。

### 真项目 USB 路径三档（按投资递增）

- **短路径 — QEMU 换 target**：stm32h743xi 等 VSF 已 port + QEMU 支持好的 USB-capable MCU（VSF 有 `vsf_usr_cfg_stm32h743xi.h` + DWCOTG dcd）。验证 VSF USB stack 逻辑首选
- **中路径（当前默认）— 真机 HIL**：任意 USB-capable MCU（stm32h7 / nrf / esp32s2）跑 `usbd_demo_msc.c` / `hidmsc`，走 ADR-002 人工 HIL 流程。真项目 90% 场景默认走这条
- **长路径 — usbip over lwip over SMSC over mps2**：`VSF_USBD_USE_DCD_USBIP` + LWIP backend + QEMU mps2 SMSC 9220 ethernet HAL port + host usbip-utils。**3-5 天独立 sprint 级**，非本 ADR 投资范围。非真项目触发不启

## 何时应重新评估

- 真实项目明确要求 linux host sim（非嵌入式 MCU 场景）
- VSF upstream 把 linux64 的 `vsf_arch_main` + `ENTRY_NO_PENDING` 实现了（linux arch 支持 kernel-only）
- `tooling/experiments/vsf-tryouts/kernel_test/` build 在 VSF 升级后失败（要么修要么删）
- `tooling/experiments/vsf-tryouts/mps2_bare/` build 或 QEMU 跑法在 VSF 升级后失败（需 re-baseline）
- 真项目需要在 sim 里跑 USB device 且 HIL / 换 target 都不合适（触发长路径评估）
- QEMU 加了 mps2-an500 的 USB 背板扩展（目前 8.2.2 没有）
- VSF 加了 mps2 HAL 的 USB port（目前没有迹象）

## 相关

- ADR-001（VSF 为基座）
- ADR-002（wheel sim 优先）
- `docs/debugging-playbook.md` § "VSF linux64 target 最小跑通配置"（配置依赖链速查）
- `docs/debugging-playbook.md` § "VSF linux64 kernel-only 模式 user_main 永不被调"（根因诊断）
- `docs/debugging-playbook.md` § "VSF mps2-an500 bare-metal sim 最小跑通配置"（首选路径 canonical checklist，5 条踩坑）
- `tooling/experiments/vsf-tryouts/kernel_test/`（linux64 build 通但 user_main 不跑的最小脚手架，作为 triage 基线）
- `tooling/experiments/vsf-tryouts/mps2_bare/`（mps2 实证跑通的 minimal scaffold，build + link + QEMU user_main 调度 + semihosting probe 全通）
- `docs/adr/006-prefer-vsf-native-over-custom-wheel.md` § Phase 0（2026-04-23 同次 USB probe 发现 `vsf_usr_cfg_<board>.h` 作"真相表"的查询法）
- `docs/debugging-playbook.md` § "预判外设可行性"（本 ADR USB 补的战术 checklist 落地）
