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
- **缺点**：拉 ~几百 MB 外部依赖；build 时间长；跑起来只是"busybox 里跑 kernel_test applet"——不是"我们掌控 VSF kernel 启动"；对真实 MCU 项目的映射不直接
- **ROI 判断**：中低。能跑起来但学到的是 busybox 用法，不是 VSF kernel

### C. 切 mps2 + QEMU 作为 kernel-only sim 目标 ✓

- **做法**：VSF 已有 `vsf/example/template/project/cmake/mps2/` target；用 QEMU `-M mps2-an385` 跑 Cortex-M3 bare-metal；kernel_test 的独立 example（`example/kernel_test/project/mdk/`）就是 bare-metal 模式
- **优点**：
  - bare-metal target 是 VSF first-class 支持（`VSF_ARCH_ENTRY_NO_PENDING` 等机制都为 MCU 设计）
  - 对真实项目映射直接（真实项目都是 MCU）
  - QEMU 跑 Cortex-M3 很轻（几百 KB），CI 友好
  - 能真正测 kernel 调度、中断、systimer 等（linux host 上这些都被 pthread 模拟）
- **缺点**：需装 QEMU（WSL apt install qemu-system-arm 30 秒）；调试反馈没 linux native 快；stdout 要经 QEMU serial 转发
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

- mps2+QEMU 自身是否 VSF 闭环 sim 路径，**今天没验证**——下次触发时仍有成本
- 如果未来项目要求 linux host sim（例如 VSF 工具链跑在 linux server 上提供 web 服务），这条 ADR 需要重评
- `tooling/experiments/vsf-tryouts/kernel_test/` 留着但不活跃——可能 VSF upgrade 后 build 不通也没人注意到

## 何时应重新评估

- 真实项目明确要求 linux host sim（非嵌入式 MCU 场景）
- VSF upstream 把 linux64 的 `vsf_arch_main` + `ENTRY_NO_PENDING` 实现了（linux arch 支持 kernel-only）
- mps2+QEMU 自身被证明也跑不通（需要 fallback plan）
- `tooling/experiments/vsf-tryouts/kernel_test/` build 在 VSF 升级后失败（要么修要么删）

## 相关

- ADR-001（VSF 为基座）
- ADR-002（wheel sim 优先）
- `docs/debugging-playbook.md` § "VSF linux64 target 最小跑通配置"（配置依赖链速查）
- `docs/debugging-playbook.md` § "VSF linux64 kernel-only 模式 user_main 永不被调"（根因诊断）
- `tooling/experiments/vsf-tryouts/kernel_test/`（build 通但 user_main 不跑的最小脚手架，作为排查基线）
