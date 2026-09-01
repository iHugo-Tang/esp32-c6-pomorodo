---
status: accepted
---

# Agent Status 仅支持 Codex Workspace

Agent Status 只接受 Codex Workspace 作为 Agent Source，不再支持 OpenCode Process。broker IPC 以 `groupId` 单独标识 Workspace，删除恒定的 `client` 字段；状态目录改为 `$XDG_STATE_HOME/pomodoro/agent-status`，不迁移旧的 `opencode/pomodoro-status` 数据。这样领域名称、设备功能与唯一实际来源保持一致，同时删除不再使用的多 client 灵活性。

## Consequences

- `OC1` 继续作为 broker 与设备之间的线路协议；协议名称和行为不变。
- 旧状态目录由用户停止旧 broker 后手动删除，新实现不自动迁移或删除用户文件。
- ADR-0010 与 ADR-0011 保留 OpenCode 集成的历史；本决策只取代 ADR-0012 的多来源类型与 IPC identity，独立 broker 和串口所有权不变。
