# wheels/modules/ — L3 功能模块

多模块协作的高层能力，通常依赖多个 L0-L2 wheels。

**合格门槛**（L3）：
- 集成测试能跑通（在 sim 里组合多 wheel）
- 必须真机验收（至少 1 个项目实战过）
- **必须参数化** —— 不绑具体实现，通过接口注入下层 wheel

**举例**：`data_logger` 不应该绑死到 `w25qxx` —— 应该接受一个"storage 接口"，项目侧传入具体实现（可以是 flash、SD 卡、内存）。

## 典型 wheel 候选

**升级**：
- `ota_updater` — OTA 固件更新（支持断点续传、回滚）
- `bootloader_base` — 通用 bootloader 骨架

**数据**：
- `data_logger` — 采集 → 存储 → 上传
- `event_recorder` — 环形事件日志

**管理**：
- `config_manager` — 配置持久化（依赖 `kvs`）
- `cli_shell` — 完整的命令行 shell（包装 `cli_parser` + 命令注册）

**网络应用**：
- `network_manager` — WiFi/BLE/以太网切换管理
- `mqtt_device_client` — MQTT + 设备影子 + OTA 触发

## 目录清单

（待添加）
