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
| 63 项静态审计（命名、NULL 守卫、错误码、轮询延时、测试平衡等） | 259/259 |
| 行覆盖率 ≥ 80% | 259/259（夜间门禁） |
| AddressSanitizer + UndefinedBehaviorSanitizer | 259/259 |
| 自动生成的 NULL 参数测试和边界值测试 | 全部公开 API |
| ARM 交叉编译（Cortex-M4，`-Os`） | 259/259，体积表在 [docs/arm-binary-sizes.md](docs/arm-binary-sizes.md) |
| libFuzzer | 25 个目标（24 个协议解析器、`json_parser`、`modbus_rtu`） |
| 提交卡口 | `make wheel-gate`：编译、测试、审计任一不过，提交被拦 |

host 之外的证据：

| 证据 | 组件 |
|---|---|
| 真机：GD32F405 遥测固件，600 s 压力测试，零看门狗复位（2026-04-27） | `crc16`、`w25qxx`、`qmi8658a`、`xgzp6816d`、`qmc5883p` |
| 寄存器表和换算公式对照参考实现（Linux 内核、厂商参考代码、Adafruit、betaflight） | 10 个 L1 驱动，清单在 [docs/known-issues.md](docs/known-issues.md) |
| 逐寄存器对照 datasheet | 抽检过一部分常见芯片和冷门芯片，结果见下节 |

## 哪里还不可信

把驱动烧上板子之前先读这一节。

- **只有上面五个组件上过真机。** 其余驱动跑的是按 datasheet 建出来的模拟寄存器模型。模型里的地址写错了，测试照样是绿的，问题要到板子上才暴露。
- **datasheet 抽检查出了真缺陷。** 常见芯片（BMP280、MPU6050、SHT30 这一类）的驱动约 16% 有问题，冷门芯片 5 个抽查 3 个有问题，类型是寄存器偏移错、少位掩码、少符号扩展。查出来的都已修复并记录在 [docs/known-issues.md](docs/known-issues.md)；没抽到的驱动应当按同样的缺陷率对待，用前自己核一遍，核对方法是 [docs/ai-workflow.md](docs/ai-workflow.md) 里的任务类型 14。
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
