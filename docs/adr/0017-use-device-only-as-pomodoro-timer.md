---
status: accepted
---

# 设备仅作为番茄钟

设备只承担离线 Pomodoro、休息、统计及其设置、显示、声音和电源行为。移除 Codex/OpenCode Agent Status 插件、独立 broker、`OC1` 状态协议、状态指示灯、确认操作与 Waiting Sound。

## Consequences

- 本决定取代 ADR-0012、ADR-0013、ADR-0014 和 ADR-0016 的 Agent Status 能力；相关 ADR 仅保留历史。
- USB-C 继续用于固件烧录、调试和 `POMO1` 只读截图，不再需要 broker 串口保留机制。
- Settings 版本 10 的两个旧开关字节保留为空闲位，保持现有 NVS journal 的布局、版本和校验方式，保存的 Timer、Set Progress、Settings 与 Statistics 继续可读。
