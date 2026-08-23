# 使用独立 Agent Status broker 独占串口

Agent Status 的来源选择、成员租约、聚合、全局 revision、`OC1` 心跳与 USB 串口所有权集中到一个零 npm 依赖的前台 Node CLI broker；OpenCode 与 Codex 只通过本地 Unix socket 上报和显式选源。这样所有来源共享一个串口边界与一致状态，而不再让各宿主进程竞选设备。

## Consequences

- 第一个有效 Agent Source 自动成为 Selected Agent Source；此后只有显式 `select` 才能切换。所选来源消失时保留选择、关闭串口并等待恢复，不自动回退。
- broker 只持久化选择与 revision，成员及 mask 使用十秒短期租约；重启后的首次恢复和每次显式选择都开启新 revision。
- broker 是适配器接入后的唯一 USB 串口持有者，并继续遵循三秒 `OC1` 心跳、设备 `ENABLED`/`DISABLED`/`ACK` 以及现有用户级 `flash-reservation`。`OC1` 线路协议与固件行为不变。
- 本决策取代 [ADR-0011](./0011-user-level-opencode-status-plugin.md) 中由 OpenCode 进程负责仲裁、全局 revision、串口所有权和自动回退的部分；用户级 OpenCode 插件及适配器迁移另行实施，本阶段不修改它。
