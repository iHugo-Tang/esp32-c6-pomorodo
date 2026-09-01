---
status: superseded by ADR-0012 and ADR-0015
---

# 通过 USB 双向同步 OpenCode Status

最初决定让 OpenCode 插件通过现有 USB Serial/JTAG 发送状态，不启用 Wi‑Fi、BLE 或常驻网络服务。状态使用有界 `OC1` 文本行；只读截图使用独立 `POMO1` 二进制响应。两个通道都不能控制 Pomodoro、修改设置或统计，也不能用于 OTA。

## Consequences

- 设备严格校验协议版本、长度和数值范围，普通日志不是协议输入。
- 状态每三秒发送一次，十秒没有有效心跳就隐藏。
- 截图最大为 480×480 RGB565；截图期间不允许文本混入二进制帧。
- 烧录工具通过存活 PID 保留协调串口，只等待有限时间，并且不向宿主进程发送停止或终止信号。

OpenCode 来源模型后来被 [ADR-0012](./0012-use-independent-agent-status-broker.md) 和 [ADR-0015](./0015-agent-status-only-supports-codex-workspaces.md) 取代；`OC1` 与 `POMO1` 的设备边界保留。
