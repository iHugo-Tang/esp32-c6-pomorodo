---
status: superseded
---

# 使用独立 Agent Status broker 独占串口

> 已被 [ADR-0017](0017-use-device-only-as-pomodoro-timer.md) 取代；以下内容仅保留为历史。

> Agent Source 类型和 IPC identity 已被 [ADR-0015](./0015-agent-status-only-supports-codex-workspaces.md) 取代；独立 broker、串口独占和 `OC1` 协议仍然有效。

Agent Status 的来源选择、成员租约、聚合、全局 revision、`OC1` 心跳和 USB 串口所有权集中到一个零 npm 依赖的 Node broker。适配器只通过本地 Unix socket 上报，不再让各宿主进程竞选设备。

## Consequences

- 第一个有效 Agent Source 自动成为 Selected Agent Source；此后只有显式 `select` 才能切换。所选来源消失时保留选择、关闭串口并等待恢复，不自动回退。
- broker 只持久化选择与 revision，成员及 mask 使用十秒短期租约；重启后的首次恢复和每次显式选择都开启新 revision。
- broker 是适配器接入后的唯一 USB 串口持有者，并继续遵循三秒 `OC1` 心跳、设备 `ENABLED`/`DISABLED`/`ACK` 以及现有用户级 `flash-reservation`。`OC1` 线路协议与固件行为不变。
- 本决策取代 [ADR-0011](./0011-user-level-opencode-status-plugin.md) 中由 OpenCode 进程负责仲裁、全局 revision、串口所有权和自动回退的部分；用户级 OpenCode 插件及适配器迁移另行实施，本阶段不修改它。
