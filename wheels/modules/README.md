# wheels/modules/ — L3 功能模块

多模块协作的高层能力，通常依赖多个 L0-L2 wheels。

**合格门槛**（L3）：
- 集成测试能跑通（在 sim 里组合多 wheel）
- 必须真机验收（至少 1 个项目实战过）
- **必须参数化** —— 不绑具体实现，通过接口注入下层 wheel

**举例**：`data_logger` 不应该绑死到 `w25qxx` —— 应该接受一个"storage 接口"，项目侧传入具体实现（可以是 flash、SD 卡、内存）。

## Origin

Proactive build (ADR-011). w


## Why self-written?

Searched GitHub (ADR-010 compliance). No suitable existing implementation found. Self-written for platform convention consistency.


## Known issues

None.


## How to test

```bash
cd wheels/modules/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_modules
```


## API convention deviations

None.


## 目录清单（20 wheels）

| Wheel | 功能 | 状态 |
|-------|------|------|
| `boot_counter` | 启动计数器（Flash 持久化） | candidate |
| `button_handler` | 按键事件处理（多按键/组合键） | candidate |
| `cli_shell` | 完整命令行 shell（命令注册+历史） | candidate |
| `command_queue` | 命令队列（异步命令调度） | candidate |
| `config_store` | 配置持久化存储 | candidate |
| `data_logger` | 数据采集/存储/上传 | candidate |
| `device_info` | 设备信息查询（型号/版本/序列号） | candidate |
| `error_handler` | 错误处理框架（错误码+回调） | candidate |
| `event_bus` | 事件总线（发布/订阅） | candidate |
| `flash_journal` | Flash 日志（追加写+磨损均衡） | candidate |
| `ota_manager` | OTA 固件更新管理 | candidate |
| `param_table` | 参数表（运行时可调参数） | candidate |
| `ring_log` | 环形日志（固定大小 RAM 缓冲） | candidate |
| `scheduler` | 任务调度器（协作式） | candidate |
| `sensor_hub` | 传感器集线器（多传感器统一采集） | candidate |
| `task_watchdog` | 任务看门狗（多任务喂狗监控） | candidate |
| `telemetry` | 遥测数据打包上报 | candidate |
| `version_info` | 版本信息管理（编译时注入） | candidate |

## 典型 wheel 候选

**升级**：
- `ota_manager` — OTA 固件更新（支持断点续传、回滚）[已实现]
- `bootloader_base` — 通用 bootloader 骨架

**数据**：
- `data_logger` — 采集 -> 存储 -> 上传 [已实现]
- `ring_log` — 环形事件日志 [已实现]
- `flash_journal` — Flash 日志（追加写 + 磨损均衡）[已实现]

**管理**：
- `config_store` — 配置持久化 [已实现]
- `cli_shell` — 完整命令行 shell（命令注册 + 历史）[已实现]
- `param_table` — 运行时可调参数 [已实现]

**网络应用**：
- `telemetry` — 遥测数据打包上报 [已实现]
- `event_bus` — 事件总线（发布/订阅）[已实现]

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
