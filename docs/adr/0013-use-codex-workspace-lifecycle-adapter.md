# 使用 Codex Workspace lifecycle adapter

Codex 通过个人全局插件 `pomodoro-agent-status` 的 lifecycle hooks 向独立 Agent Status broker 上报。Agent Source 以 Codex Workspace 为边界：Git 目录使用规范化仓库根目录，非 Git 目录使用规范化工作目录；同一来源内的 chat、fork、side chat 与 subagent 由 broker 按成员 mask 聚合。

## Considered Options

- 使用已弃用的 custom prompt 或伪造原生 `/pomodoro-select`：命令入口不稳定，也不能可靠排除控制 turn 的状态周期。
- 让每个 hook 直接向 broker 上报：hook 结束后没有存活 PID 可承担十秒租约，也无法在 broker 重启后保留和恢复内存状态。
- 每个主 session 启动轻量 reporter，并以受支持的 `$pomodoro-select` skill 选择 Workspace。

## Consequences

- reporter 自身 PID 承担十秒成员租约，每三秒续租；它监控最近的 Codex 祖先进程，在 SessionEnd 或宿主退出时发送 `remove` 并退出。
- hooks 与 reporter 使用短路径、权限受限的临时 Unix socket；只有确认没有监听者时才清理陈旧 socket。broker 不可用或响应损坏不会阻塞 Codex，reporter 保留状态并重试。
- hooks 映射五态并锁存可观察到的结构化工具 Error；`compact` 不重置，真实 prompt/tool 工作清除 Error，subagent 以独立成员加入和移除。
- `$pomodoro-select` 由 `UserPromptSubmit` hook 完成 report 与 `select` 后把结果交给同名 skill；skill 不调用工具。控制 turn 不上报 Working 或 Completed，重复选择仍只由 broker 开启一个新 revision。
- 插件不启动或后台化 broker，不创建 LaunchAgent，不新增 npm 依赖，也不修改 broker 的 `report`、`select`、`remove` schema。
- 安装后的 hooks 必须由用户通过 `/hooks` 审查并信任；源码以本仓库版本为准，个人插件目录仅为安装副本。
