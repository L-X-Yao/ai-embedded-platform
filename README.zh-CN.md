# ai-embedded-platform（中文说明）

259 个零依赖的 C99 嵌入式组件，30 天内用 AI 驱动的工作流写成，每个组件都要通过一条自动化验证流水线才能入库。英文版 [README.md](README.md) 是完整文档，这一页只讲四件事：它是什么、验证到什么程度、哪里还不可信、怎么做出来的。

## 它是什么

- 四层：L0 原语 77 个、L1 驱动 115 个、L2 协议 47 个、L3 模块 20 个，都在 `wheels/`，完整清单见 [wheels/README.md](wheels/README.md)，按总线找驱动见 [docs/mcu-compatibility.md](docs/mcu-compatibility.md)。
- 被动库：不碰 OS、不分配堆、不建任务。硬件访问全部通过函数指针注入，所以同一份源码在 Cortex-M、RISC-V、x86 上都能编译，也能在电脑上跑测试。
- 用法：把 `.c/.h` 拷进工程，注入 HAL 回调，调 `_init`，然后用 API。例子见英文 README 的 Quick start。

## 验证到什么程度

每个组件入库前都要过的门：

| 门 | 范围 |
|---|---|
| host 单元测试 | 259/259，1431 个测试文件，11138 个 `TEST()` 用例 |
| 60 项静态审计（命名、NULL 守卫、错误码、轮询延时、测试平衡等） | 259/259 |
| 行覆盖率 ≥ 80% | 259/259（夜间门禁） |
| AddressSanitizer + UndefinedBehaviorSanitizer | 259/259，全部 931 个测试目标（每晚） |
| 自动生成的 NULL 参数测试和边界值测试 | 全部公开 API |
| ARM 交叉编译（Cortex-M4，`-Os`） | 259/259，体积表在 [docs/arm-binary-sizes.md](docs/arm-binary-sizes.md) |
| libFuzzer | 57 个目标（42 个协议、11 个原语、3 个模块、1 个驱动），解析和编码都覆盖 |
| 提交卡口 | `make wheel-gate`：编译、测试、审计任一不过，提交被拦 |

host 之外的证据：

| 证据 | 组件 |
|---|---|
| 真机：GD32F405 遥测固件，600 s 压力测试，零看门狗复位（2026-04-27） | `crc16`、`w25qxx`、`qmi8658a`、`xgzp6816d`、`qmc5883p` |
| 寄存器表和换算公式对照参考实现（Linux 内核、厂商参考代码、Adafruit、betaflight） | 10 个 L1 驱动，清单在 [docs/known-issues.md](docs/known-issues.md) |
| 逐寄存器对照 datasheet | 2026 年 11 月查了 82 个驱动，此前有一批抽检，结果见下节 |

## 哪里还不可信

把驱动烧上板子之前先读这一节。

- **只有上面五个组件上过真机。** 其余驱动跑的是模拟寄存器模型，其中不少模型和驱动一起写成，把同样的错误也写了进去，所以测试是绿的不代表寄存器值是对的，问题要到板子上才暴露。
- **datasheet 抽检查出了真缺陷，比例还很高。** 常见芯片（BMP280、MPU6050、SHT30 这一类）的驱动约 16% 有问题，冷门芯片 5 个抽查 3 个有问题，类型是寄存器偏移错、少位掩码、少符号扩展。2026 年 11 月换了更严的逐项核对，查了全部 104 个有寄存器或协议的驱动，确认约 515 条缺陷。其中约 295 条在 84 个驱动的代码里，从产品 ID 写错到 CRC 表让真机读取失败都有；约 220 条在测试模型里，另有 14 个驱动只有模型有问题。先前判定没问题的驱动也查出了缺陷。不用改 HAL 或大改的驱动缺陷已修复，但这些修复同样没上过真机；测试模型只修了驱动修复用到的部分；`bmi270`、`vl53l1x`、`vl53l0x` 标为不可用；`w5500` 还不支持 TCP 和 UDP；`w25qxx`、`qmi8658a`、`qmc5883p`、`xgzp6816d`、`air780eg` 查过但没改，等有板子能复测再说。详见 [docs/known-issues.md](docs/known-issues.md)。
- **不带 RTOS，也不带 MCU HAL。** 总线回调由你提供。`vsf-addon/` 里有一个 GD32F4xx 的 VSF HAL 移植作为完整示例，`vsf/` 是为它保留的上游原样快照。
- **许可证。** 组件是 MIT；`wheels/primitives/kvs/vendor/` 的 FlashDB 和 `vsf/` 快照是 Apache-2.0，各自保留原有声明。

## 怎么做出来的

- **时间**：2026-04-21 到 05-20，约 1170 次提交。一名嵌入式工程师指挥 Claude Code，经常是几个 agent 在不同的 git worktree 里并行。
- **分工**：人定需求、审 API、做架构决策（[12 个 ADR](docs/adr/README.md)）、做审计和真机测试；AI 实现、写测试、写文档、跑工具链。
- **约束 AI 的文件**：[CLAUDE.md](CLAUDE.md) 是 agent 的常驻工作规范，[docs/wheel-contract.md](docs/wheel-contract.md) 定义组件必须是什么样，[docs/api-conventions.md](docs/api-conventions.md) 定命名和错误处理，[docs/ai-workflow.md](docs/ai-workflow.md) 是 14 类任务的做法，[docs/debugging-playbook.md](docs/debugging-playbook.md) 是踩过的坑，防止再踩。
- **工具链为什么存在**：生成很快，一个典型驱动从脚手架到测试全绿约 15 分钟，上一节的缺陷率就是"快"在没有检查时的代价。`tooling/` 下的每个工具，都是为某一类已经漏过去一次的 bug 补上的。
- **没有公开的部分**：本仓库是内部 monorepo 的公开副本。消费这些组件的客户固件（`projects/`）不公开，文档里提到 `projects/flybox`、`projects/drone_light` 的地方指的就是它。

## 作者

[L-X-Yao](https://github.com/L-X-Yao)，嵌入式工程师，做过飞控、工业设备控制和整车控制器。欢迎 issue 和 PR，其他联系方式见主页。
