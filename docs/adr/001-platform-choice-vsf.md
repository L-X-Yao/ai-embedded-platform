# ADR 001: 平台选择 VSF 作为基座

**状态**：accepted
**日期**：2026-04-21
**决策人**：@L-X-Yao

## 背景

构建 AI 驱动嵌入式开发平台，需要选一个 RTOS/框架作为基座。这个基座决定后续所有 wheels、templates 的技术绑定。候选至少需满足：
- 支持 USB / BLE / WIFI / USB / 文件系统 / GUI 等常见嵌入式能力
- 有 host 仿真可做自动化测试
- 支持多 MCU（STM32/ESP32/国民/兆易/先辑等）
- 许可证允许商用

## 考虑过的选项

### VSF（Versaloon Software Framework）
- 优点：
  - USB 主从机栈深度（作者 Simon Qian 是 Versaloon USB 专家）
  - "皮肤"机制（linux skin 含 49 POSIX headers 含 libusb）——独特架构
  - 统一 HAL 抽象（同一套 API 跨芯片 SPI/GPIO 等）
  - 编译快（20s 完整编译）
  - 二进制小（820KB sim）
  - PLOOC OOP 架构现代
  - Apache 2.0
- 缺点：
  - 训练数据少，AI 学习成本高
  - 文档稀疏（README + PPT）
  - 有静默失败场景
  - 贡献者集中度高（3 人）
  - 国民技术/兆易/先辑 VSF 原生无 HAL port

### NuttX（Apache 顶级项目）
- 优点：
  - POSIX 原生（AI 母语）
  - sim target 成熟（96 个 configs）
  - 社区规模大（100+ 贡献者，Xiaomi/Sony 等企业贡献）
  - 错误信息标准
  - 文档相对齐全
- 缺点：
  - 编译慢（2-3 min）
  - 二进制大（4.3MB sim）
  - USB/BLE/GUI 不是核心定位
  - 小资源 MCU 支持不如 VSF

### Zephyr
- 优点：DTS 硬件建模强、Twister 测试框架
- 缺点：POSIX 覆盖只有 24 headers（不如 VSF 49 或 NuttX 原生），构建系统重

### FreeRTOS + 手搓
- 优点：训练数据最多
- 缺点：只是内核，要自己组栈（lwip/fatfs/tinyusb），集成风险高

## 决策

**选 VSF 作为主基座**，理由：
1. 用户场景重 USB/BLE/GUI/小资源 MCU，这是 VSF 的 sweet spot
2. "皮肤"机制让应用层代码（POSIX 风格）可跨平台移植
3. 统一 HAL 对多 MCU 支持友好
4. AI 学习成本通过"平台积累文档 + memory 沉淀"可逐步摊销
5. 用户已评估接受其 bus factor（见 ADR 007）

## 后果

### 好
- 深度绑定 VSF 的独特优势（USB/皮肤/HAL）
- 编译快、二进制小带来高 AI 迭代频率
- Apache 2.0 兼容商用

### 坏
- AI 需要"驯化"（前 1-2 个月补文档/模板成本）
- 客户强指定 NuttX/Zephyr 时需走 VSF 皮肤适配
- 多 MCU 需自行写 HAL port（国民/兆易/先辑等）

## 何时应重新评估

- VSF 项目活跃度骤降（>6 个月无 commit），且 AI 接管维护后遇到跨不过的架构问题
- 用户团队扩到 10 人以上，训练新人 VSF 成本过高
- 业务重心完全转向 VSF 弱势场景（例：汽车/医疗认证——应选 ThreadX）
- 出现新一代 AI 不再需要 wheels 抽象（直接生成一切），wheels 策略本身过时

## 相关

- ADR-007（VSF 永久基座决策）
- `docs/philosophy.md`（平台愿景）
- RTOS 评估历史排序：NuttX > VSF > Zephyr（记录于 Claude Code 自动记忆）
