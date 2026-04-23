# log

**STATUS: draft**
**Layer: L0 (primitives)**

带等级 + 模块标签 + 多 sink + 编译期过滤的日志 wheel，封装在 `vsf_trace` 之上。

## 为什么不用 VSF 原生？

已检查 VSF 模块：

- `vsf_trace`（`service/trace/`）：✓ 提供等级（NONE/ERROR/WARNING/INFO/DEBUG）、printf 风格、彩色、stream 输出、buffer hex dump。**覆盖 ~60% 需求**。
- `vsf_bgtrace`（同文件）：✓ 类型化后台 ring buffer trace，用于高频追踪场景。与本 wheel 定位不冲突。

**VSF 原生缺的 4 个能力**（嵌入式产品常需）：
1. **模块标识** — 日志里分不出 BLE / driver / app 来源
2. **多 sink 同时输出** — vsf_trace 绑定单一 stream，不能 UART + flash + 远程并行
3. **编译期 level 过滤** — vsf_trace 是运行时等级，DEBUG 字符串仍编译进 release binary
4. **时间戳** — vsf_trace 不自动加时间前缀

**结论**：本 wheel **在 vsf_trace 之上封装**，不替换。默认 sink (`log_add_vsf_trace_sink`) 直接 bypass vsf_trace 自身格式化、把我们组好的行 `__vsf_trace_output()` 输出到 vsf_trace 绑定的 stream。复用 vsf_trace 的成熟 stream + 锁保护，不重造底层 IO。

## How to test

```bash
cd wheels/primitives/log

# 默认构建（LOG_LEVEL_MAX=LOG_LEVEL_DEBUG，所有级别都输出）
VSF_PATH=/mnt/d/temp/project/ai-embedded-platform/vsf cmake -GNinja -S . -B build
cmake --build build

# release 风格构建（LOG_LEVEL_MAX=LOG_LEVEL_WARN，DEBUG/INFO 被编译器消除）
# 这个构建用于验证：DEBUG/INFO 的宏真的扩展成 ((void)0)，参数副作用不求值
VSF_PATH=/mnt/d/temp/project/ai-embedded-platform/vsf cmake -GNinja -S . -B build_release -DCMAKE_C_FLAGS='-DLOG_LEVEL_MAX=LOG_LEVEL_WARN'
cmake --build build_release

python3 -m pytest test_log.py -v
```

预期：**11 个 C 测试 + 4 个 pytest 用例全绿**（含 release 构建编译期消除验证）。

## API 摘要

```c
/* 每个 .c 顶部声明模块名 */
#define LOG_MODULE "mymodule"
#include "log.h"

/* 四个主宏 */
LOG_ERROR("...");
LOG_WARN("...");
LOG_INFO("...");
LOG_DEBUG("...");

/* 编译期过滤（release 可设 LOG_LEVEL_WARN 去掉 DEBUG/INFO 字符串） */
#define LOG_LEVEL_MAX  LOG_LEVEL_DEBUG

/* 运行时 */
log_set_level(LOG_LEVEL_INFO);
log_add_vsf_trace_sink();            /* 预置：路由到 vsf_trace 的 stream */
log_add_stream_sink(&my_stream);     /* 任意 VSF stream */
log_add_sink(my_sink_fn, ctx);       /* 自定义 */
log_set_time_fn(my_ms_fn);           /* NULL 可关时间戳 */
```

## 输出格式

```
[00012345] [I] [ble] connected to peer 0a:1b:2c
[00012367] [W] [driver] i2c retry #1
[00012389] [E] [app] config crc mismatch
```

- `[XXXXXXXX]` 时间戳（ms，8 位对齐），可关
- `[L]` 等级字符 `E/W/I/D`
- `[module]` 调用处 `LOG_MODULE` 值

## 配置宏（可在项目侧覆盖）

| 宏 | 默认 | 说明 |
|---|-----|------|
| `LOG_LEVEL_MAX` | `LOG_LEVEL_DEBUG` | 编译期最高等级（高于此级的字符串被优化掉） |
| `LOG_MAX_SINKS` | 4 | 最多注册的 sink 数 |
| `LOG_LINE_BUFSIZE` | 256 | 每行最大字节（含前缀 + 消息 + 换行） |

## 设计取舍

| 点 | 选择 | 原因 |
|----|------|------|
| 模块标识 | `#define LOG_MODULE` 每文件手写 | 最简单、不依赖链接器、编译期常量可参与优化 |
| 编译期过滤 | `if ((level) <= LOG_LEVEL_MAX)` | 编译器能把 DEAD 分支 + 字符串一起消除 |
| Sink 注册 | 动态列表（runtime） | 灵活、多项目可插拔；空闲时零开销 |
| vsf_trace 集成 | bypass 其格式化，复用其 stream 输出 | 避免双重前缀、复用成熟锁保护 |
| 线程安全 | 每次 log_emit 用栈 buffer 256B | 无共享状态，无需锁；代价是栈使用 |
| 结构化日志 | 暂不做 | 一般嵌入式产品不需要 JSON 日志 |

## 依赖

- VSF kernel
- `VSF_USE_SIMPLE_STREAM = ENABLED`
- `VSF_USE_TRACE = ENABLED`（若用 `log_add_vsf_trace_sink`）
- libc `snprintf` / `vsnprintf`（linux skin 带 simple_libc 即有）

## 支持的平台

- [x] VSF linux host (sim) — 测试通过 2026-04-21
- [x] VSF mps2-an500 bare-metal (QEMU) — 2026-04-22 via custom semihosting sink，见 `tooling/experiments/vsf-tryouts/mps2_bare/`
- [ ] VSF + STM32F4 — 未验证
- [ ] VSF + ESP32 — 未验证

（L0 wheel 不强求真机，但可以在首个项目里顺便打勾。）

## 已知坑

（暂无；发现后追加）

## 测试状态

- 单元测试（sim）：PASS（10/10 C 测试 + 3/3 pytest）
- 硬件在环：N/A

## 用法示例

### 基础

```c
#include "log.h"
#define LOG_MODULE "app"

int main(void) {
    log_add_vsf_trace_sink();           /* 默认路由到 vsf_trace */
    log_set_level(LOG_LEVEL_INFO);
    LOG_INFO("boot ok version=%d.%d", 1, 2);
    return 0;
}
```

### 多 sink

```c
#include "log.h"
#define LOG_MODULE "ble"

extern vsf_stream_t flash_log_stream;

void ble_init(void) {
    log_add_stream_sink(&flash_log_stream);   /* 日志同时存 flash */
    LOG_INFO("ble stack started");
}
```

### 自定义 sink

```c
static void my_net_sink(void *ctx, const char *line, size_t len) {
    /* 发到远程 MQTT topic 等 */
    (void)ctx;
    mqtt_publish("device/log", line, len);
}

log_add_sink(my_net_sink, NULL);
```

## Responsible human

@user（2-3 人团队，暂）

## API Review

- 提案日期：2026-04-21
- 审批人：@user
- 关键决策：采纳所有默认推荐（Q1-Q5 全按 AI 建议执行）

## 升级到 candidate 前要做

- [ ] 至少 1 个真项目使用过
- [ ] README 补 "性能参考" 节（log_emit 耗时 / RAM 占用）
- [ ] README 补 "变更历史" 节
- [ ] 跨平台 打勾（至少 STM32/ESP32 之一真机跑过）
- [ ] 评估 rtdbg.h 3 条参考点（`_eval/rtthread_rtdbg/`）：ANSI 彩色（需 per-sink 方案）/ `LOG_HERE`（函数+行号）/ `LOG_RAW`（跳过前缀）。**均属新 API**，真实项目触发后独立走 wheel-contract §7 审批。hex dump 不自造，走 VSF `vsf_trace_dump_buff`（ADR-006）。
