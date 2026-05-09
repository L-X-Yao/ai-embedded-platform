# vsf-addon/

**新增**到 VSF 的独立代码——不是修改 vsf/ 既有文件。

## 策略

见 `docs/adr/005-vsf-upstream-modification-policy.md`。

规则：
- 修改 vsf/ 既有文件 → `tooling/ci/patches/` 放 patch
- **新增独立文件 / 目录 → 这里**（保持 vsf/ 干净）

## 目录结构

```
vsf-addon/
├── hal/
│   └── driver/
│       ├── national-tech/   国民技术 HAL port（按需增加）
│       ├── gd/              兆易创新
│       └── sino-wealth/     先辑
├── component/               新组件（按需）
└── example/                 新示例（按需）
```

目录结构**镜像 vsf/source/**，方便路径认知迁移。

## 使用

项目 CMakeLists.txt 里，在 include VSF 后加：

```cmake
include($ENV{VSF_PATH}/script/cmake/vsf.cmake)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../vsf-addon)
```

或按需只引用具体子目录：
```cmake
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../vsf-addon/hal/driver/gd)
```

## 每个 addon 子模块必须有

- `README.md` — 说明它是什么、何时用、支持的硬件
- `CMakeLists.txt` — 能被 add_subdirectory 纳入
- 遵循 VSF HAL 接口（对于驱动）
- 至少 sim 下有 mock 测试（如果是可 sim 的）
- 真机验收记录（在 README 里）

## 当前 addon 清单

| 路径 | 描述 | 状态 | 验证平台 |
|------|------|------|---------|
| hal/driver/gd/GD32F4XX | GD32F405 MCU HAL port (GPIO) | draft | GD32F405RGT6 — 待真机验证 |
