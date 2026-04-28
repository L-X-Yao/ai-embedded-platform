# API 约定：跨 wheel 一致性

**定位**：这是**跨 wheel 的风格统一规则**。目的是整个平台的 wheels 看起来像一套东西，而不是各说各话。

- 这个文档回答"怎么写 API"
- `wheel-contract.md` 回答"wheel 什么时候能过审"

每个 wheel 必须遵守本文档。偏离条款需在 wheel README 的"Convention Deviations"节说明理由。

## 1. 命名

### 1.1 函数命名

```
<wheel_name>_<verb>[_<qualifier>]
```

- ✅ `mpu6050_init`, `mpu6050_read_accel`, `mpu6050_set_range`
- ✅ `log_write`, `log_set_level`
- ❌ `init_mpu6050`（动词在前）
- ❌ `readAccel`（驼峰）

### 1.2 标准动词

| 动词 | 含义 |
|------|------|
| `init` | 初始化（调用方分配 context 后调用） |
| `fini` / `deinit` | 反初始化（释放内部资源，**不**释放 context） |
| `open` / `close` | 使用/结束使用（与 init/fini 配对时表示"一次会话"） |
| `read` / `write` | 数据读写 |
| `get` / `set` | 查询/配置属性 |
| `start` / `stop` | 启动/停止持续行为（timer、polling 等） |
| `enable` / `disable` | 开启/关闭功能（不销毁状态） |
| `reset` | 重置内部状态 |
| `is_<状态>` | 布尔查询 |

### 1.3 类型命名

```
<wheel_name>_t           主 context 类型
<wheel_name>_cfg_t       配置结构
<wheel_name>_handle_t    如果是句柄式接口（不透明指针）
<wheel_name>_<sub>_t     子结构
```

- ✅ `mpu6050_t`, `mpu6050_cfg_t`, `mpu6050_accel_t`
- ❌ `MPU6050`, `mpu6050_config`（大写或不加 _t）

### 1.4 常量/宏

```
<WHEEL_NAME>_<CATEGORY>_<NAME>
```

- ✅ `MPU6050_RANGE_2G`, `MPU6050_MAX_SAMPLE_RATE`
- ❌ `RANGE_2G`（无 wheel 前缀）

## 2. 错误处理

### 2.1 返回值约定

按函数语义分三类：

**(a) 纯状态函数** — 用 `vsf_err_t`，复用 VSF 错误码空间

```c
vsf_err_t mpu6050_init(mpu6050_t *dev, const mpu6050_cfg_t *cfg);
// 返回: VSF_ERR_NONE (0) / -VSF_ERR_NOT_READY / -VSF_ERR_FAIL / ...
```

**(b) 返回有意义数值的函数** — 正数是有效值，可能用负数或 0 表示错误（POSIX 风格）

```c
size_t ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, size_t len);
// 返回: 实际写入字节数（可能 < len；0 = 写不进去，满了）

int log_add_sink(log_sink_fn fn, void *ctx);
// 返回: sink id >=0，或 -1 失败（POSIX 风格，类比 open/socket）

int some_fd_open(const char *path);
// 返回: fd >=0，或 -1 失败
```

**何时选 (b) 不选 (a)**：函数本质要**返回一个值**（id / size / count / fd）。硬塞 `vsf_err_t` 就得用 out-parameter，难用。

**(c) 布尔函数** — 用 `bool`（命名 `xxx_is_xxx`）

```c
bool ring_buffer_is_empty(ring_buffer_t *rb);
```

**原则**：不自造错误码空间——要么 `vsf_err_t`，要么 POSIX 风格 -1。不要写"自己一套 `MYWHEEL_ERR_*` 枚举"。

### 2.2 不用 `errno` 全局变量

嵌入式多任务环境下 errno 容易错乱。**错误直接通过返回值传**。

### 2.3 断言 vs 返回错误

- **编程错误**（API 误用、NULL 指针等不该发生的情况）→ `VSF_ASSERT()`，不通过返回值传
- **运行时错误**（硬件无响应、buffer 满等可恢复情况）→ 返回错误码

## 3. 内存所有权

### 3.1 Context 由调用方分配

```c
// ✅ 对
mpu6050_t dev;
mpu6050_init(&dev, &cfg);

// ❌ 不要 malloc
mpu6050_t *dev = mpu6050_create(&cfg);  // 禁止这种风格，除非 wheel 本质需要动态
```

**例外**：如果 wheel 内部需要动态数据结构（如可变长 list），只动态分配内部部分，context 本身仍调用方分配。

### 3.2 Buffer 由调用方提供

```c
// ✅ 对：调用方给 buffer
uint8_t storage[64];
ring_buffer_init(&rb, storage, sizeof(storage));

// ❌ 不要 wheel 自己分配 buffer
ring_buffer_init(&rb, 64);  // 禁止——wheel 不应自行 malloc
```

### 3.3 字符串 / 数据的生命周期

- `set` 类函数传入的指针**不保留**（wheel 内部需要就 copy）
- `get` 类函数返回的指针**不许外部释放**，且生命周期等同于 context
- 回调函数传入的指针**仅在回调内有效**，要留用必须 copy

## 4. 线程安全

每个公开函数**必须在头文件注释里声明线程安全属性**：

```c
/**
 * Read accelerometer data.
 *
 * @thread_safe: yes            // 可并发调用
 * @thread_safe: no             // 不可并发，调用方保证
 * @thread_safe: caller_must_lock  // wheel 不锁，调用方要加锁
 */
int mpu6050_read_accel(mpu6050_t *dev, int16_t *x, int16_t *y, int16_t *z);
```

**约定**：
- L0/L1 wheel 大多 `@thread_safe: caller_must_lock`（不内置锁）
- L2/L3 wheel 更多 `@thread_safe: yes`（内置 VSF 锁，对调用者透明）

**如果 wheel 需要锁**：用 `vsf_mutex_t` + `vsf_eda_mutex_enter/leave`。不要用 pthread_mutex（VSF 皮肤下才有，L1 驱动用会把非 VSF_USE_LINUX 环境拖下水）。

## 5. 回调 / 钩子

### 5.1 回调签名统一

```c
typedef void (*<wheel>_cb_t)(void *ctx, <wheel>_event_t event, void *data);
```

- 第一参数必须是 `void *ctx`（用户私有数据）
- 第二参数是事件/消息类型
- 数据参数可选

### 5.2 回调上下文

- 是否允许在中断里调用？**头文件注释必须写明**
- 回调内可调用哪些 wheel API？**必须列出限制**（通常禁止调用会引发回调循环的函数）

## 6. 配置

### 6.1 编译期配置 vs 运行时配置

- **编译期**（`Kconfig` / 宏）：功能大开关、大小上限
- **运行时**（`<wheel>_cfg_t`）：具体参数

**原则**：能编译期决定的不做运行时，减少 RAM 占用。

### 6.2 cfg 结构的设计

```c
typedef struct {
    // 必填项在前
    uint8_t i2c_addr;
    const vsf_i2c_t *i2c;
    // 可选项在后，有合理默认
    uint16_t sample_rate_hz;  // 0 = 默认
    uint8_t range;             // MPU6050_RANGE_DEFAULT = 0
} mpu6050_cfg_t;
```

- 默认值用 `0` / `NULL` 表达"用默认"
- 不要用魔数（用带 wheel 前缀的 enum/define）

## 7. 日志

**所有 wheel 不直接调用 printf**——用平台级 `log` 模块：

```c
#include "log.h"
LOG_INFO("mpu6050 init ok, wai=0x%02x", wai);
LOG_ERROR("mpu6050 read timeout");
```

- `LOG_DEBUG` — 开发时临时
- `LOG_INFO` — 重要生命周期事件
- `LOG_WARN` — 可恢复异常
- `LOG_ERROR` — 关键失败

每个 wheel 在 `.c` 顶部：
```c
#define LOG_MODULE "mpu6050"
```

## 8. 文件头

每个 wheel 的 `.c` 和 `.h` 顶部：

```c
/*
 * <wheel_name> — <一句话功能描述>
 *
 * Layer:   L0 | L1 | L2 | L3
 * Status:  draft | candidate | stable
 * Deps:    <逗号分隔的其他 wheel>
 */
```

## 9. 依赖

完整规则见 **[ADR-009](adr/009-wheel-dependency-rules.md)**（单一来源）。摘要：

- **方向**：只能向下（L3→L2→L1→L0→无），禁止向上，同层需说明理由
- **具体依赖**：直接 `#include`，仅用于唯一实现的算法（如 modbus→crc16）。文件头 `Deps:` 列 wheel 名
- **接口依赖**：HAL callback 注入，用于可替换的服务（如 kvs→flash_hal）。文件头 `Deps:` 列 `<名>_hal`
- **判据**：只有一种正确实现→具体依赖；多种实现都合理→接口依赖
- **禁止循环**

## 10. 版本

wheel 内部不写死版本号。版本 = git tag（如 `wheel/mpu6050/v0.2.0`）。

---

## 11. 偏离约定的处理

**偏离约定需明确理由**：在 wheel 的 README 写一节 "Convention Deviations"，列出哪些约定没遵守、为什么。

审批：API review 环节人工判定。
