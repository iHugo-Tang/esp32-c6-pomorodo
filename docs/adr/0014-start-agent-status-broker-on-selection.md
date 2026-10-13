---
status: superseded
---

# 选择 Agent Source 时按需启动 broker

> 已被 [ADR-0017](0017-use-device-only-as-pomodoro-timer.md) 取代；以下内容仅保留为历史。

Codex 的 `$pomodoro-select` 在 Agent Status broker socket 没有监听者时，以 detached Node 子进程启动插件内的 broker；已有实例时直接复用。broker 的唯一实现随插件安装，仓库 `tools/pomodoro-agent-status-broker.mjs` 只保留为薄前台入口。

## Consequences

- 只有选择控制事件负责自动启动；普通 lifecycle hook 在 broker 不可用时继续静默重试，不创建 LaunchAgent 或 PID 文件。
- 并发选择依靠现有 Unix socket 排他监听收敛为单实例，竞选失败的进程直接退出。
- 自动实例在启动后或最后一次严格校验成功的 Agent `report` 后十秒退出；`select`、`remove`、无效 IPC 与串口 `OC1` 心跳不续命。退出会关闭串口、客户端 socket 与监听 socket。
- 手工运行薄前台入口不启用空闲退出，继续由 `SIGINT`/`SIGTERM` 清理。
- 本决策取代 [ADR-0013](./0013-use-codex-workspace-lifecycle-adapter.md) 中“插件不启动 broker”的约束，不修改 `report`、`select`、`remove` schema，也不新增 npm 依赖。
