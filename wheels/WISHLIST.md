# Wheels Wishlist（高置信候选，非 TODO）

> ⚠️ **强警告**
>
> - 这**不是** TODO / 不是路线图 / 不是承诺。
> - 只是"未来 2-3 个项目**大概率**会触发"的候选——触发时从这里挑作为起点。
> - **没真实项目触发前不要做任何一个**（见 ADR-006 + `docs/ai-workflow.md` 任务类型 1）。
> - 条目已裁剪到高置信范围（约 10 项）。其他发散想法**不预先列**——真实触发时现场 grep VSF + `ls wheels/` 就够了。
>
> **维护规则**：
> - 某个候选 3 次以上在真实项目里被触发但还没做 → 优先做
> - 3 个项目过后仍没触发 → 从本表删除
> - 没真实信号就加的候选不进本表（进不进不影响记忆，靠 memory + git log 就够）

---

## L0 · 工具原语

| 候选 | 功能 | VSF 原生检查 |
|------|------|-------------|
| `kvs` | Key-Value 配置存储（flash 抽象） | VSF 有 `component/fs/` 但无专门 KV 封装 |

## L1 · 硬件驱动

| 候选 | 功能 | VSF 原生检查 |
|------|------|-------------|
| `gpio_led` | LED 状态机：闪烁 / 呼吸 / 脉冲 / 组合 pattern | VSF 无（HAL 只提供 GPIO 原语） |
| `gpio_button` | 按键消抖 + 长按 / 双击 / 组合键 | VSF 无 |
| `ws2812` | WS2812B RGB LED 驱动（timer PWM bit-bang） | VSF 无 · 触发：drone_light (×1) |
| `ssd1306` | OLED（I2C，调试常用） | VSF 有 tgui 但底层驱动需自写 |

## L2 · 协议 / 服务

| 候选 | 功能 | VSF 原生检查 |
|------|------|-------------|
| `at_command` | AT 指令解析器（WiFi / 4G / BLE 模组通用） | VSF 无专门 AT 引擎 |
| `mqtt_client` | MQTT 3.1.1 客户端（IoT 场景必备） | VSF 无；选型 paho / mongoose / mqttnet |
| `modbus_rtu_master` | Modbus RTU 主站（工业项目典型） | VSF 无 |
| `modbus_rtu_slave` | Modbus RTU 从站 | VSF 无 |

## L3 · 功能模块

| 候选 | 功能 | VSF 原生检查 |
|------|------|-------------|
| `ota_updater` | 固件升级（版本检查 / 断点续传 / 回滚 / 签名） | VSF 有 bootloader 素材但无集成 OTA |

---

## 添加新候选的格式

只在**有真实项目信号**时添加。格式：

```markdown
- `<wheel-name>` — <一句话功能> | VSF 原生：<有/无/部分> | 触发项目：<哪个项目> | 信号次数：N
```

触发 N ≥ 1 才进本表。无触发信号的"好主意"不进——心理暗示"有很多要做"是反模式。

---

## 实现流程（触发后）

真项目遇到需求 → 按 `ai-workflow.md` 任务类型 1 走：
1. grep VSF 原生
2. 写 "Why not VSF native?"
3. API 提案给用户 15 分钟审批
4. 放 `wheels/_draft/<name>/`
5. TDD + sim 验证
6. 交回升级审批
