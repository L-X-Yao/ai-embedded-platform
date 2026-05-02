# API 约定：跨 wheel 一致性

**定位**：这是**跨 wheel 的风格统一规则**。目的是整个平台的 wheels 看起来像一套东西，而不是各说各话。

- 这个文档回答"怎么写 API"
- `wheel-contract.md` 回答"wheel 什么时候能过审"
- `architecture.md` 回答"为什么这么设计"

每个 wheel 必须遵守本文档。偏离条款需在 wheel README 的"Convention Deviations"节说明理由。

## 0. 核心原则：Wheel 是被动库

**Wheel 永远是被调用的，永远不主动做任何事。** 完整原则见 `architecture.md`，这里列操作性规则：

- **不创建任务/线程** — 应用层负责
- **不注册中断** — ISR 设 flag，任务里调用 wheel
- **不调用 sleep/delay** — 需要延时通过 HAL 回调注入 `delay_ms`
- **不分配堆内存** — context 和 buffer 由调用方分配（见 §3）
- **不加锁** — 线程安全由调用方保证（见 §4）
- **不 `#include` 其他 wheel** — 依赖通过回调注入（见 §10）
- **不 `#include` 任何 OS 头文件** — 零 OS 依赖

判断标准：如果一个功能需要"自己创建任务"或"自己管理中断"，它不应该是 wheel。

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
<wheel_name>_t           主 context 类型（强制）
<wheel_name>_cfg_t       配置结构
<wheel_name>_err_t       错误枚举
<wheel_name>_i2c_t       HAL 注入结构
<wheel_name>_data_t      输出数据结构
<wheel_name>_<sub>_t     其他子结构
```

- ✅ `bmp280_t`, `bmp280_cfg_t`, `bmp280_data_t`
- ❌ `bmp280_dev_t`（不用 `_dev_t`，主 context 统一为 `_t`）
- ❌ `MPU6050`, `mpu6050_config`（大写或不加 `_t`）

### 1.4 常量/宏

```
<WHEEL_NAME>_<CATEGORY>_<NAME>
```

- ✅ `MPU6050_RANGE_2G`, `MPU6050_MAX_SAMPLE_RATE`
- ❌ `RANGE_2G`（无 wheel 前缀）

### 1.5 Accepted prefix abbreviations

The following wheels use shortened prefixes for API usability. These are documented deviations, not violations:

| Wheel directory | Prefix used | Reason |
|----------------|-------------|--------|
| `fixed_point` | `fp_` | Standard mathematical convention |
| `protobuf_lite` | `pb_` | Universal protobuf abbreviation |
| `hash_fnv` | `fnv1a_` | Algorithm name is the prefix |
| `lookup_table` | `lut_` | Standard abbreviation |
| `lora_sx1276` | `lora_` | Chip model is implementation detail |
| `nrf24l01` | `nrf24_` | Chip model is implementation detail |
| `button_handler` | `btn_handler_` | Reasonable shortening |
| `version_info` | `version_` | Info is redundant in API names |
| `keypad_matrix` | `keypad_` | Matrix is redundant in API names |
| `cbor_lite` | `cbor_lite_enc_`/`cbor_lite_dec_` | Encoder/decoder distinction |

New wheels should use the full directory name as prefix. Abbreviation requires a note in the wheel's README "Convention Deviations" section.

### 1.6 物理单位后缀（强制）

返回物理量的结构体字段和函数名**必须**用后缀标明单位，消除歧义：

| 后缀 | 单位 | 示例 |
|------|------|------|
| `_c` | 摄氏度 | `temperature_c` |
| `_pa` | 帕斯卡 | `pressure_pa` |
| `_ms` | 毫秒 | `delay_ms`, `timeout_ms` |
| `_us` | 微秒 | `delay_us`, `pulse_us` |
| `_hz` | 赫兹 | `sample_rate_hz` |
| `_mv` | 毫伏 | `voltage_mv` |
| `_ma` | 毫安 | `current_ma` |
| `_deg` | 度（角度） | `heading_deg` |
| `_dps` | 度/秒（角速度） | `gyro_dps` |
| `_mg` | 毫g（加速度） | `accel_mg` |
| `_lux` | 勒克斯 | `illuminance_lux` |
| `_ppm` | ppm | `concentration_ppm` |
| `_raw` | 原始 ADC / 寄存器值 | `temperature_raw` |
| `_pct` | 百分比 | `humidity_pct` |
| `_g` | 重力加速度（G） | `ax_g` |
| `_gauss` | 高斯（磁通量密度） | `mx_gauss` |
| `_ohm` | 欧姆（电阻） | `gas_resistance_ohm` |
| `_m` | 米 | `altitude_m` |
| `_mm` | 毫米 | `distance_mm` |
| `_kmh` | 千米/小时 | `speed_kmh` |

- ✅ `bmp280_data_t { float temperature_c; float pressure_pa; }`
- ❌ `bmp280_data_t { float temperature; float pressure; }`（单位不明）

无量纲的整数计数（`count`、`index`、`slot`）不需要后缀。

## 2. 错误处理

### 2.1 返回值约定

按函数语义分三类：

**(a) 纯状态函数** — 用 wheel 级错误枚举

```c
typedef enum {
    BMP280_OK        =  0,
    BMP280_ERR_I2C   = -1,
    BMP280_ERR_ID    = -2,
    BMP280_ERR_PARAM = -3,
} bmp280_err_t;

bmp280_err_t bmp280_init(bmp280_t *dev, const bmp280_i2c_t *i2c,
                         const bmp280_cfg_t *cfg);
```

命名：`<WHEEL_NAME>_OK = 0`，其余 `<WHEEL_NAME>_ERR_<原因>`。OK 必须为 0。

**为什么不用 `vsf_err_t`**：wheels 设计为可脱离 VSF 独立编译（host 测试、其他 RTOS 复用），依赖 `vsf_err_t` 会引入不必要的 VSF 耦合。Wheel 级枚举还能携带 domain-specific 错误语义（如 `ERR_I2C` vs `ERR_ID`）。

**(b) 返回有意义数值的函数** — 正数是有效值，可能用负数或 0 表示错误（POSIX 风格）

```c
size_t ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, size_t len);
// 返回: 实际写入字节数（可能 < len；0 = 写不进去，满了）

int log_add_sink(log_sink_fn fn, void *ctx);
// 返回: sink id >=0，或 -1 失败（POSIX 风格，类比 open/socket）
```

**何时选 (b) 不选 (a)**：函数本质要**返回一个值**（id / size / count / fd）。硬塞错误枚举就得用 out-parameter，难用。

**(c) 布尔函数** — 用 `bool`（命名 `xxx_is_xxx`）

```c
bool ring_buffer_is_empty(ring_buffer_t *rb);
```

**原则**：每个 wheel 用自己的 `<name>_err_t` 枚举。不用 `vsf_err_t`（可移植性），也不用裸 int（无类型安全）。Vendor wheel 例外——沿用上游 API 的错误码风格。

**符号约定**：`_OK = 0`，所有错误码为**显式负值**（`_ERR_X = -1, _ERR_Y = -2`）。禁止自增（`_ERR_X,`），因为自增产生正值，`if (err < 0)` 检查会失效。

### 2.2 标准错误码集（强制单数）

新 wheel 优先从此表选择错误码名，不要发明新名。确实没覆盖的可以新增，但命名格式必须是 `_ERR_<REASON>`，**单数**。

| 错误码 | 含义 | 适用层 |
|--------|------|--------|
| `_OK` | 成功，**必须 = 0** | 全部 |
| `_ERR_PARAM` | 参数无效（NULL / 越界 / 不合法） | 全部 |
| `_ERR_I2C` | I2C 通信失败 | L1 |
| `_ERR_SPI` | SPI 通信失败 | L1 |
| `_ERR_HAL` | 通用 HAL 失败（非 I2C/SPI 时） | L1 |
| `_ERR_ID` | 芯片 ID 校验失败 | L1 |
| `_ERR_TIMEOUT` | 超时 | L1, L2 |
| `_ERR_CRC` | 校验和失败 | L0, L2 |
| `_ERR_FORMAT` | 协议格式错误 | L2 |
| `_ERR_MALFORMED` | 数据结构损坏 | L2 |
| `_ERR_SHORT` | 数据/buffer 不足 | L2 |
| `_ERR_OVERFLOW` | 数据超出容量 | L0, L2 |
| `_ERR_FULL` | 容器/表已满 | L0, L3 |
| `_ERR_EMPTY` | 容器/队列为空 | L0 |
| `_ERR_NOT_READY` | 设备未就绪 | L1 |
| `_ERR_BUSY` | 设备忙 | L1 |

- ✅ `RING_BUFFER_ERR_PARAM`（单数）
- ❌ `RING_BUFFER_ERR_PARAMS`（复数）

### 2.3 不用 `errno` 全局变量

嵌入式多任务环境下 errno 容易错乱。**错误直接通过返回值传**。

### 2.4 NULL 指针守卫（强制）

所有公开 API 函数**必须在入口检查指针参数**。wheel 代码运行在裸机 ISR 或 RTOS 任务中，NULL 解引用直接 HardFault。

```c
// ✅
w25qxx_err_t w25qxx_read(const w25qxx_spi_t *spi, ...) {
    if (!spi || !buf) return W25QXX_ERR_SPI;
    ...
}

// ❌ 裸机上 assert 通常是死循环，不如返回错误码让调用方处理
w25qxx_err_t w25qxx_read(const w25qxx_spi_t *spi, ...) {
    VSF_ASSERT(spi != NULL);  // 不做——用返回值
    ...
}
```

### 2.5 Float NaN 守卫（强制）

接受 `float` 参数的公开 API 函数**必须**拒绝 NaN。IEEE 754 的 NaN 使所有 `<` `>` 比较返回 false，能绕过任何 `val < min || val > max` 范围检查。

```c
// ❌ NaN 绕过（NaN < 0 是 false，NaN > 100 也是 false）
if (duty < 0.0f || duty > 100.0f) return ERR_PARAM;

// ✅ 正确写法：取反正向判断（NaN >= 0 也是 false，所以 !false = true → 拒绝）
if (!(duty >= 0.0f && duty <= 100.0f)) return ERR_PARAM;

// ✅ 或者前置 NaN 检查
if (val != val) return ERR_PARAM;  /* C 中 NaN != NaN 恒为 true */
```

### 2.6 断言 vs 返回错误

- **运行时错误**（硬件无响应、buffer 满等可恢复情况）→ 返回错误码
- **内部不变量**（只有代码 bug 才能触发）→ `VSF_ASSERT()`（不在公开 API 边界使用）

### 2.7 L2 协议 build/encode 返回值

L2 协议的 `_build()` / `_encode()` 函数统一返回 `int`：

```c
int mqtt_packet_build_connect(uint8_t *buf, size_t buf_size,
                              const mqtt_connect_opts_t *opts);
// 返回: >0 = 写入字节数（成功），-1 = 参数错误，-2 = buffer 不够
```

| 返回值 | 含义 |
|--------|------|
| `> 0` | 成功，返回写入字节数 |
| `-1` | 参数错误（NULL 指针、无效字段） |
| `-2` | buffer 不够（`ERR_SHORT`） |

**判断规则**：build 只有 2 种失败（NULL / buffer 不够）→ 用返回值区分。有 3+ 种失败 → 用 `_err_t` + `size_t *out_len`。

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

### 3.4 deinit/fini 安全性契约（强制）

`deinit` / `fini` 函数**必须**安全处理以下三种场景：

1. **zero-init struct**：`memset(&dev, 0, sizeof(dev))` 后调用 `deinit` 不崩溃
2. **半初始化 struct**：`init` 中途失败后调用 `deinit` 不崩溃
3. **重复调用**：连续两次 `deinit` 不崩溃（幂等）

实现方式：`deinit` 内部检查关键指针是否为 NULL，为 NULL 直接返回 `_OK`。

```c
// ✅
bmp280_err_t bmp280_deinit(bmp280_t *dev) {
    if (!dev) return BMP280_ERR_PARAM;
    if (!dev->i2c) return BMP280_OK;  /* not initialized or already deinit'd */
    /* ... cleanup ... */
    dev->i2c = NULL;
    return BMP280_OK;
}
```

## 4. 线程安全

每个公开函数**必须在头文件注释里声明线程安全属性**，从以下三种标注中选一个：

| 标注 | 含义 | 适用场景 |
|------|------|----------|
| `@thread_safe: caller_must_lock` | 可并发，调用方需加锁 | 读写 context struct 的数据操作函数 |
| `@thread_safe: yes` | 无需锁即可并发 | 无状态纯函数（L2 parse/build、L0 编解码） |
| `@thread_safe: no` | **禁止并发调用** | 改变初始化状态的生命周期函数 |

**判断规则**：
- 函数读写 context struct 的数据字段 → `caller_must_lock`
- 函数不碰任何可变状态（纯输入→输出）→ `yes`
- 函数改变 context 的初始化状态（init / deinit / reset）→ `no`

```c
/** Initialize and verify chip ID. @thread_safe: no */
bmp280_err_t bmp280_init(bmp280_t *dev, const bmp280_i2c_t *i2c,
                         const bmp280_cfg_t *cfg);

/** Read temperature and pressure. @thread_safe: caller_must_lock */
bmp280_err_t bmp280_read(bmp280_t *dev, bmp280_data_t *out);

/** Parse CONNACK from raw bytes. @thread_safe: yes */
mqtt_packet_err_t mqtt_packet_parse_connack(const uint8_t *buf, size_t len,
                                            mqtt_connack_t *out);
```

**Wheel 不加锁**——锁是 OS 原语，wheel 不碰 OS（见 §0）。如果项目需要线程安全，在应用层用 RTOS 的 mutex 包装调用。

### 4.1 ISR 安全

独立于 `@thread_safe`，标注函数是否可从中断服务程序（ISR）中调用：

| 标注 | 含义 |
|------|------|
| `@isr_safe: yes` | 可从 ISR 调用（不阻塞、不调用 HAL 回调、不碰 stdio） |
| `@isr_safe: no` | 不可从 ISR 调用（默认，可省略） |

```c
/** Push one byte. @thread_safe: caller_must_lock @isr_safe: yes */
ring_buffer_err_t ring_buffer_push(ring_buffer_t *rb, uint8_t byte);

/** Initialize with I2C HAL. @thread_safe: no @isr_safe: no */
bmp280_err_t bmp280_init(bmp280_t *dev, ...);
```

**大多数 wheel 函数都是 `@isr_safe: no`**（被动库通常不在 ISR 中使用）。只有少数数据结构（ring_buffer、event_queue）的 push/pop 操作可能需要标 yes。

`@isr_safe: no` 是默认值，可以省略不写。只有 `@isr_safe: yes` 必须显式标注。

**声称 `@isr_safe: yes` 的函数必须满足**：
- 不调用任何 HAL 回调（不做 I2C/SPI/GPIO 操作）
- 不调用 stdio（不 printf）
- 不阻塞（无循环等待、无 delay）
- 如果共享字段在 ISR 和任务间读写 → 该字段必须标 `volatile`

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

### 5.3 HAL 注入标准签名（L1 驱动）

L1 驱动通过 callback struct 注入 HAL。按 I2C 协议模式分三种标准签名，**新 wheel 必须选其中之一**：

**Pattern A — register-based**（BMP280, MPU6050, QMI8658A, QMC5883P, HMC5883L）

```c
typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} <wheel>_i2c_t;
```

用于标准 8-bit 寄存器寻址设备。`delay_ms` 为可选回调——需要等待的 wheel（如 EEPROM 写入、自校准）使用它，不需要的 wheel 调用方传 NULL 或不设此字段（C 零初始化）。

如果设备有 16-bit 寄存器（如 ADS1115 config），额外添加 `write_regs` 回调：

```c
    bool (*write_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                       const uint8_t *buf, uint16_t len);
```

**Pattern B — command-based**（SHT30, BH1750）

```c
typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr,
                  const uint8_t *data, uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr,
                 uint8_t *data, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} <wheel>_i2c_t;
```

用于 Sensirion 等发送 command word（非寄存器地址）的设备。

**Pattern C — SPI**（MAX7219）

> **注**：W25QXX 使用单字节 `xfer` + cs_low/cs_high 签名，见其 `w25qxx.h`——比 Pattern C 的 `transfer` 批量接口更简单，是合理的偏离。

```c
typedef struct {
    bool (*transfer)(void *ctx, const uint8_t *tx, uint8_t *rx, uint16_t len);
    void (*cs_low)(void *ctx);
    void (*cs_high)(void *ctx);
    void *ctx;
} <wheel>_spi_t;
```

**其他 HAL**：GPIO（`set_output/set_input/write_pin/read_pin/delay_us`）、1-Wire（`reset/write_byte/read_byte`）、PWM（`set_pulse_us`）。`new_wheel.sh` 的 `--hal` 参数对应以上全部模式。

**选择依据**：开始写新 L1 驱动前先查 datasheet 的总线协议，不要盲用 register-based 模板。

### 5.4 HAL 回调 struct 存储方式（强制）

wheel 的 context struct **存 `const` 指针**指向调用方提供的 HAL struct，**不拷贝**整个 struct：

```c
// ✅ 存指针
typedef struct {
    const bmp280_i2c_t *i2c;   /* caller-owned, must outlive dev */
} bmp280_t;

// ❌ 拷贝值
typedef struct {
    bmp280_i2c_t i2c;          /* 浪费 RAM，且不能共享 */
} bmp280_t;
```

理由：
- 函数指针 struct 通常定义为 `static const`，生命周期没问题
- 存指针节省 RAM（一个含 5 个函数指针的 struct 在 32-bit MCU 上占 20+ bytes）
- 多个 wheel 实例可共享同一个 HAL struct（如两个 BMP280 共用一条 I2C 总线）

### 5.5 HAL 回调阻塞时间声明（强制）

如果 wheel 内部调用 `delay_ms` 或其他可能阻塞的 HAL 回调，**必须**在头文件函数注释中声明最大阻塞时间：

```c
/** Reset and recalibrate. Calls delay_ms (max 300ms).
 *  @thread_safe: no @isr_safe: no */
drv2605_err_t drv2605_calibrate(drv2605_t *dev);

/** Read temperature. Non-blocking (no delay_ms call).
 *  @thread_safe: caller_must_lock @isr_safe: no */
bmp280_err_t bmp280_read(bmp280_t *dev, bmp280_data_t *out);
```

消费者据此决定是否能在实时任务中调用该函数。

## 6. 配置

### 6.1 编译期配置 vs 运行时配置

- **编译期**（`Kconfig` / 宏）：功能大开关、大小上限
- **运行���**（`<wheel>_cfg_t`）：具体参数

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

### 6.3 config.h 放置规则

`config.h` 是可选的编译期配置文件。放置规则：

- `config.h` **只被 `.c` 文件 include**，不被 `.h` 文件 include
- 消费者通过 `-D<WHEEL>_CONFIG_PATH="myconfig.h"` 或直接 `-D<MACRO>=<value>` 传入配置
- 禁止通过 include path 覆盖 `config.h`（会导致不同 TU 使用不同配置，引发 ODR 违规）

```c
// ✅ wheel.c 中
#ifdef CLI_SHELL_CONFIG_PATH
#include CLI_SHELL_CONFIG_PATH
#endif

// ❌ wheel.h 中 include config.h（暴露给所有消费者，可能冲突）
```

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

**例外**：L0/L1 wheel 的 host sim 测试构建不强制依赖 `log` 模块——这些 wheel 设计为零依赖或仅依赖 HAL callback，在 sim 测试中不需要日志。真机集成时由项目层决定是否接入 `log`。

## 8. 文件头

每个 wheel 的 `.c` 和 `.h` 顶部：

```c
// SPDX-License-Identifier: MIT
/*
 * <wheel_name> — <一句话功能描述>
 *
 * Layer:     L0 | L1 | L2 | L3
 * Status:    draft | candidate | stable
 * Deps:      <依赖列表，格式见下>
 * Instances: multi | single
 */
```

### 8.1 Deps 格式

| 类型 | 格式 | 示例 |
|------|------|------|
| 无依赖 | `(none)` | `Deps: (none)` |
| 接口依赖（HAL 回调注入） | `<name>_hal (injected)` | `Deps: i2c_hal (injected)` |
| 具体依赖（直接 #include） | `<name>` | `Deps: crc16` |
| 混合 | 逗号分隔 | `Deps: crc16, i2c_hal (injected)` |

### 8.2 Instances 字段

| 值 | 含义 |
|----|------|
| `multi` | 每个 `_t` 实例独立，可同时创建多个（默认，大多数 wheel） |
| `single` | 有进程级全局状态，不支持多实例（如 `log` 的全局 sink 表） |

### 8.3 SPDX 许可证标识

所有 self-written wheel 文件**第一行**写 `// SPDX-License-Identifier: MIT`。

Vendor wheel 沿用上游许可证（已在 `VENDOR.md` 追踪），不加 SPDX 行。

## 9. C++ 兼容与头文件守卫（强制）

### 9.1 `extern "C"` 守卫

所有 wheel 的 `.h` 文件**必须包含 `extern "C"` 守卫**：

```c
#ifdef __cplusplus
extern "C" {
#endif

/* ... 全部声明 ... */

#ifdef __cplusplus
}
#endif
```

放在 `#include` 之后、`#endif /* GUARD */` 之前。目的：wheel 可能被 C++ 项目（VSF 组件、测试框架）直接 include。`audit_wheels.py` 自动检查。

### 9.2 Include 守卫风格

**使用 `#ifndef` / `#define` / `#endif`，禁止 `#pragma once`**。

```c
// ✅
#ifndef BMP280_H
#define BMP280_H
/* ... */
#endif /* BMP280_H */

// ❌
#pragma once  // 非 C99 标准，symlink 构建场景会失败
```

守卫宏命名：`<WHEEL_NAME_UPPER>_H`。

## 10. 依赖

完整规则见 **[ADR-009](adr/009-wheel-dependency-rules.md)**（单一来源）。摘要：

- **方向**：只能向下（L3→L2→L1→L0→无），禁止向上，同层需说明理由
- **具体依赖**：直接 `#include`，仅用于唯一实现的算法（如 modbus→crc16）。文件头 `Deps:` 列 wheel 名
- **接口依赖**：HAL callback 注入，用于可替换的服务（如 kvs→flash_hal）。文件头 `Deps:` 列 `<名>_hal (injected)`
- **判据**：只有一种正确实现→具体依赖；多种实现都合理→接口依赖
- **禁止循环**

## 11. 版本

wheel 内部不写死版本号。版本 = git tag。

**Tag 格式**：`wheel/<name>/v<MAJOR>.<MINOR>.<PATCH>`

```bash
git tag -a "wheel/crc16/v0.1.0" -m "crc16 v0.1.0: initial candidate release"
git push origin --tags
```

- `v0.1.0` = 首次 candidate release
- MAJOR 升级 = API 破坏性变更
- MINOR 升级 = 新功能，向后兼容
- PATCH 升级 = bug fix

项目可通过 tag 锁定 wheel 版本（monorepo 内用 commit hash 引用即可，tag 作为可读标记）。

### 11.1 API 废弃

废弃函数使用编译器属性标注，至少保留一个 MINOR 版本的过渡期：

```c
__attribute__((deprecated("use bmp280_read_data instead")))
bmp280_err_t bmp280_read(bmp280_t *dev, bmp280_data_t *out);
```

废弃流程：标注 `deprecated` → MINOR 版本发布 → 下一个 MAJOR 版本删除。

---

## 12. 偏离约定的处理

**偏离约定需明确理由**：在 wheel 的 README 写一节 "Convention Deviations"，列出哪些约定没遵守、为什么。

审批：API review 环节人工判定。

## 13. 编译期安全守护

### 13.1 `_Static_assert`（C11）

如果 wheel 内部有以下任一操作，**必须**用 `_Static_assert` 守护架构假设：

- packed struct（`__attribute__((packed))`）
- 指针强转（`uint8_t *` → `uint32_t *`）
- 位域操作假设特定字节序
- buffer 大小假设（如 power-of-2 要求）

```c
// ✅
_Static_assert(sizeof(uint32_t) == 4, "requires 32-bit int");
_Static_assert(sizeof(my_packet_t) == 12, "packed struct size mismatch");

// 对于 C99 兼容（不支持 _Static_assert 的编译器）：
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(my_packet_t) == 12, "packed struct size mismatch");
#endif
```

大多数 L0/L1 wheel 不需要 `_Static_assert`。主要用于 L2 协议 wheel 的 wire-format struct。
