# ADR 009: Wheel 依赖规则

**状态**：accepted
**日期**：2026-04-28
**决策人**：@L-X-Yao

## 背景

平台前 11 个 wheel 全部零互依赖——每个独立编译、独立测试。但随着 WISHLIST 推进，跨 wheel 依赖不可避免：

- `modbus_rtu` (L2) 需要 `crc16` (L0)——Modbus 协议规定 CRC16-MODBUS 校验
- `kvs` (L0) 需要 flash 读写擦——由 `w25qxx` (L1) 或内部 Flash 提供
- `ota_updater` (L3) 需要 crc16 + flash + transport

当前 `api-conventions.md` 有 `Deps:` 文件头字段，`wheel-contract.md` 有依赖节，但**没有规则约束依赖方向和类型**。不定规则，后续 wheel 可能出现 L0 直接 `#include` L1 头文件（如 kvs 直接用 w25qxx）、同层循环依赖等架构腐化。

## 考虑过的选项

### A. 保持全部零依赖（每个 wheel 通过 callback 注入一切）

- 优点：最大解耦，每个 wheel 独立测试无需拉其他 wheel
- 缺点：modbus 需要 `crc16_func` callback 毫无意义——CRC16-MODBUS 只有一种实现，callback 是纯粹的间接性浪费

### B. 允许任意依赖，靠文档约束

- 优点：最灵活
- 缺点：无强制方向，架构会腐化（L0 依赖 L1、循环依赖等）

### C. 区分两种依赖类型 + 方向规则（选中）

- 优点：精确匹配嵌入式实际需求——算法直接用，硬件/服务通过接口注入
- 缺点：需要判断"用具体依赖还是接口依赖"，但判据清晰（见下）

## 决策

**选 C：两种依赖类型 + 严格方向规则。**

### 规则 1：依赖方向——只能向下

```
L3 modules    → 可依赖 L2, L1, L0
L2 protocols  → 可依赖 L1, L0
L1 drivers    → 可依赖 L0
L0 primitives → 不依赖任何 wheel
```

**同层依赖**：不推荐，允许但需在 README "Convention Deviations" 节说明理由。
**向上依赖**：禁止。违反即架构 bug。

### 规则 2：两种依赖类型

**(a) 具体依赖（Concrete）**——直接 `#include`，编译时链接

适用条件：**只有一种合理实现**。

| 例子 | 为什么是具体依赖 |
|------|-----------------|
| modbus → crc16 | Modbus 协议规定 CRC16-MODBUS，没有第二种算法 |
| ota → crc16 | 完整性校验用固定算法 |
| data_logger → log | 平台只有一个日志系统 |

规则：
- 只允许向下流（高层 → 低层）
- 消费方 `.c` 直接 `#include "crc16.h"`
- 消费方 `test/CMakeLists.txt` 用相对路径编译依赖 `.c`
- 文件头 `Deps:` 字段列出具体依赖

**(b) 接口依赖（Interface）**——HAL callback 注入，项目集成时接线

适用条件：**多种实现都合理**。

| 例子 | 为什么是接口依赖 |
|------|-----------------|
| kvs → flash 操作 | 后端可以是 w25qxx、内部 Flash、SD 卡 |
| mqtt → TCP 传输 | 传输可以是 WiFi、蜂窝、以太网 |
| w25qxx → SPI 总线 | SPI 实现因 MCU 而异 |
| gpio_led → GPIO 控制 | GPIO 实现因 MCU 而异 |

规则：
- 通过 `xxx_hal_t` 结构体注入函数指针
- wheel 不知道具体实现，不 `#include` 实现方的头文件
- 测试用 mock 实现
- 文件头 `Deps:` 字段写 `<接口名>_hal`（如 `flash_hal`），不写具体 wheel

**判据**：问"这个功能是否只有一种正确实现？"是→具体依赖；否→接口依赖。

### 规则 3：接口依赖不算向上

kvs (L0) 通过 callback 使用 flash 操作，项目集成时接线到 w25qxx (L1)。这**不是** L0→L1 向上依赖，因为 kvs 不知道 w25qxx 的存在。类比：POSIX `read()` 不依赖 ext4，虽然运行时 ext4 提供实现。

### 规则 4：禁止循环

如果 A→B 且 B→A（无论具体还是接口），说明共享逻辑应下沉到更低层。

## 构建约定

### 具体依赖的 test/CMakeLists.txt

```cmake
# modbus_rtu/test/CMakeLists.txt
add_executable(test_modbus_rtu
    test_modbus_rtu.c
    ../modbus_rtu.c
    ../../../primitives/crc16/crc16.c    # 具体依赖：相对路径
)
target_include_directories(test_modbus_rtu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../primitives/crc16
)
```

### 接口依赖的 test/CMakeLists.txt

```cmake
# kvs/test/CMakeLists.txt
add_executable(test_kvs
    test_kvs.c
    ../kvs.c
    mock_flash.c                          # 接口依赖：mock
)
```

## 后果

### 好
- 依赖方向明确，架构不会随 wheel 增多而腐化
- 两种类型覆盖嵌入式全部实际场景
- 具体依赖零开销（直接函数调用），接口依赖可 LTO 内联
- 与现有 HAL callback 模式完全兼容，不需要改已有 wheel

### 坏
- 新 wheel 作者需判断"具体 vs 接口"——但判据简单
- 相对路径编译在目录重构时需同步修改——但 monorepo 内可控

## 何时应重新评估

- 跨 wheel 具体依赖超过 10 条 → 考虑 CMake `find_package` / 统一构建层
- 出现同层循环依赖需求且无法下沉 → 重新审视分层模型
- wheel 数量超过 30 且目录结构频繁变动 → 考虑 CMake 变量替代相对路径

## 相关

- `docs/api-conventions.md` §8（文件头 `Deps:` 字段）
- `docs/wheel-contract.md` §2（结构要求）
- ADR-006（优先 VSF 原生）
- ADR-004（Monorepo 结构）
