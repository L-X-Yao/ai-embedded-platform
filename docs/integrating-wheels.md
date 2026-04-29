# Wheel 接入指南

如何在项目中消费 `wheels/` 中的可复用模块。

## 基本模式

Wheel 不是独立 CMake 子项目，而是 **.h + .c 文件对**。项目通过 `target_include_directories` + `target_sources` 引入。

### 纯 CMake 项目

```cmake
# 假设项目 CMakeLists.txt 在 projects/<proj>/
set(PLATFORM_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

# 1. 头文件搜索路径
target_include_directories(my_app PRIVATE
    ${PLATFORM_ROOT}/wheels/primitives/crc16
    ${PLATFORM_ROOT}/wheels/drivers/w25qxx
    ${PLATFORM_ROOT}/wheels/protocols/at_command
)

# 2. 编译源文件
target_sources(my_app PRIVATE
    ${PLATFORM_ROOT}/wheels/primitives/crc16/crc16.c
    ${PLATFORM_ROOT}/wheels/drivers/w25qxx/w25qxx.c
    ${PLATFORM_ROOT}/wheels/protocols/at_command/at_command.c
)
```

### VSF-based 项目（参考 flybox）

VSF 项目同样使用标准 CMake 命令（不需要 `vsf_add_*` 宏）：

```cmake
# projects/flybox/vsf_build/CMakeLists.txt 实际模式
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
    ${PLATFORM_ROOT}/wheels/primitives/crc16
    ${PLATFORM_ROOT}/wheels/drivers/qmi8658a
)
target_sources(${CMAKE_PROJECT_NAME} PUBLIC
    ${PLATFORM_ROOT}/wheels/primitives/crc16/crc16.c
    ${PLATFORM_ROOT}/wheels/drivers/qmi8658a/qmi8658a.c
)
```

### Keil MDK / Makefile 项目

直接把 wheel 的 `.c` 加入源文件列表，`.h` 所在目录加入 include path。无 CMake 依赖。

## HAL 适配

大多数 L1 驱动 wheel 通过 callback struct 注入硬件操作：

```c
/* I2C 驱动 wheel（qmi8658a / qmc5883p / xgzp6816d）*/
static bool my_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val) {
    /* 调用你的平台 I2C API */
}
static bool my_read_regs(void *ctx, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len) {
    /* ... */
}
static const qmi8658a_i2c_t imu_i2c = {
    .write_reg = my_write_reg,
    .read_regs = my_read_regs,
    .ctx = NULL,
};
qmi8658a_init(&imu_i2c, &cfg);

/* SPI 驱动 wheel（w25qxx）*/
static const w25qxx_spi_t flash_spi = {
    .cs_low  = my_cs_low,
    .cs_high = my_cs_high,
    .xfer    = my_spi_xfer,
    .ctx = NULL,
};
w25qxx_init(&flash_spi, &jedec_id);
```

## 跨 wheel 依赖

部分 wheel 依赖其他 wheel（如 `modbus_rtu` 依赖 `crc16`）。wheel 的 README `## Dependencies` 节会注明。你需要同时引入依赖 wheel 的 .h 和 .c。

## 查找可用 wheel

```bash
ls wheels/primitives/ wheels/drivers/ wheels/protocols/ wheels/modules/ wheels/_draft/
python3 tooling/scripts/audit_wheels.py  # 查看完整清单 + 状态
```

每个 wheel 的 README 有 API 示例和使用说明。

## 相关文档

- [wheel-contract.md](wheel-contract.md) — wheel 分级标准 + 各阶段验收清单
- [api-conventions.md](api-conventions.md) — 跨 wheel 命名/错误码/内存/线程约定
- [ADR-009](adr/009-wheel-dependency-rules.md) — 依赖方向规则（只向下，禁循环）
