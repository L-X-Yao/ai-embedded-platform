# templates/

项目起步模板。新项目从这里拷贝到 `projects/<name>/`，然后调整。

## 目录

（暂无模板；第一个模板预计在第 1-2 个真实项目之后抽取出来。）

## 模板的合格定义

一个 template 要满足：

1. **能直接编译跑通** —— 无需修改就能在 sim 里出结果
2. **包含 CLAUDE.md 骨架** —— AI 进入项目就知道约定
3. **包含至少一个 "hello world" 测试** —— 验证脚手架本身工作
4. **README 说明适用场景** —— 什么情况下该用这个模板

## 规划中的模板（需要真实项目孵化）

- `minimal/` —— 最小 VSF kernel 项目（基于 `_eval/demo_wsl` 演化）
- `ble-peripheral/` —— BLE 从机设备
- `usb-device/` —— USB 设备（CDC / HID / MSC 可选）
- `data-logger/` —— 传感器采集 → 存储 → 上传
- `gui-app/` —— 基于 LVGL 或 TGUI 的界面应用
