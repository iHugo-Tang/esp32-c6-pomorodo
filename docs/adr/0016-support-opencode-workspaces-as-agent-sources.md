---
status: superseded
---

# 支持 OpenCode Workspace 作为独立 Agent Source

> 已被 [ADR-0017](0017-use-device-only-as-pomodoro-timer.md) 取代；以下内容仅保留为历史。

OpenCode 1.18.29 已提供 lifecycle events 和可直接执行 slash action 的 TUI plugin 接口，因此 Agent Status 可以在保留现有 Codex 集成的同时支持 OpenCode Workspace。OpenCode 与 Codex 即使指向同一代码库也使用不同 Agent Source identity；两者可以同时上报，但只有 Selected Agent Source 影响设备，显式选择 OpenCode 后 Codex 状态不再参与当前显示。

OpenCode 通过用户级 server plugin 把 Workspace 内全部存活 Session 与 subagent 上报给现有独立 broker，通过用户级 TUI plugin 提供 `/pomodoro-select`。该命令必须直接选择当前 Workspace，不创建 prompt、Session message、Task 或模型请求；普通 OpenCode 工作不能在首次选择前自动激活来源。所选 Workspace 的成员全部消失时保留选择，等待同一 Workspace 恢复，不自动回退。

## Consequences

- broker 继续独占 USB、聚合成员、分配 revision、维持租约并协调烧录；OpenCode 不直接访问串口，`OC1` 和设备固件无需改变。
- OpenCode 使用与 Codex 一致的五态优先级。busy/retry 为 Working，权限或 Question 等待为 Waiting，Session 或结构化工具失败锁存 Error，成功 idle 为 Completed，新真实工作清除对应成员 Error。
- `/pomodoro-select` 首版只承诺 OpenCode TUI。1.18.29 的直接 slash registration 当前可用但已标记 deprecated，因此升级验证必须检测接口失效，且不得静默退化为会触发模型的 Markdown command。
- 插件失败、broker 不可用、状态目录错误或串口不可用不得阻塞 OpenCode；选择失败保留原来源并只显示不含工作内容的反馈。
- 本决定取代 [ADR-0015](./0015-agent-status-only-supports-codex-workspaces.md) 的 Codex-only 来源边界，也取代 [ADR-0014](./0014-start-agent-status-broker-on-selection.md) 中「不修改 `report`、`select`、`remove` schema」的后续影响：这三类请求现在接受可选且严格校验的 `host` 字段（缺省 `codex`，仅接受 `codex` 或 `opencode`）。ADR-0012 的独立 broker、ADR-0013 的 Codex adapter 和 ADR-0014 的按需启动原则继续有效。
