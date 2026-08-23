# Independent Agent Status CLI Broker

## Goal

用一个零 npm 依赖的 Node broker 汇集 OpenCode 与 Codex 的 Agent Status。broker 是唯一 USB 串口持有者；适配器只通过本地 IPC 上报，不创建 LaunchAgent。

## Source selection

- Agent Source 由 `{client, groupId}` 标识，`client` 只接受 `opencode` 或 `codex`。
- 第一个包含存活成员的来源自动成为 Selected Agent Source；之后只有显式 `select` 改变选择，重复选择同一来源也开启新 revision。
- `select` 只接受当前至少有一个存活成员的来源。
- 所选来源失去全部成员时保留选择、停止状态心跳并释放串口。其他来源不得自动接管；所选来源首次恢复时开启新 revision。

## IPC

- broker 唯一实现位于 `plugins/pomodoro-agent-status/scripts/broker.mjs`；`tools/pomodoro-agent-status-broker.mjs` 是保持常驻行为的薄前台入口。
- socket 为 `${XDG_STATE_HOME:-$HOME/.local/state}/opencode/pomodoro-status/agent-status.sock`；目录权限 `0700`，socket 权限 `0600`。
- 活跃 broker 已占用 socket 时第二实例立即失败；只有确认没有监听者时才移除陈旧 socket。
- IPC 使用 JSON line，每行最多 8 KiB：
  - `{type:"report", client:"opencode"|"codex", groupId, memberId, pid, startedAt, mask}`
  - `{type:"select", client, groupId}`
  - `{type:"remove", client, groupId, memberId}`
  - 响应 `{ok, selected:{client,groupId}|null, revision, error?}`
- 请求必须严格匹配对应字段；标识符非空且限长，PID 必须为存活正整数，`startedAt` 为正的毫秒时间戳，`mask` 为 `0..15`。未知类型、额外或缺失字段、损坏 JSON 与超长行只拒绝当前请求。

## Leases, aggregation, and revision

- `report` 同时续租成员；十秒未刷新或 PID 消失即移除。来源 mask 是全部存活成员 mask 的按位或，设备继续按 Error、Waiting、Working、Completed、Idle 的既有优先级解析。
- 首次选中、显式选择、所选来源首次恢复，以及仍存活的所选来源聚合 mask 改变时递增 revision。重复 report 与三秒设备心跳不递增。
- `agent-coordination.json` 以原子替换只持久化 `{selected, revision}`；成员和 mask 不持久化。重启后必须由新 `report` 重建来源。
- revision 耗尽、状态文件损坏或状态目录不可安全读写时 fail closed：不覆盖损坏数据，也不发布状态。

## Codex Workspace adapter

- 个人全局插件 `pomodoro-agent-status` 通过受信任的 Codex lifecycle hooks 上报；Git 仓库内按规范化 Git 根目录聚合，非 Git 目录按 `realpath(cwd)` 聚合。
- 每个主 session 启动一个 reporter，以 hash 后的 Workspace 路径和 `session_id` 作为 `groupId` 与主 `memberId`；reporter 自身 PID 续租，每三秒重报并在 Codex 宿主祖先进程退出时清理。
- hooks 通过权限为 `0700/0600` 的短路径临时 Unix socket 更新 reporter；broker 不可用时状态留在内存并静默重试，不阻塞 Codex。
- 状态映射为：普通 prompt 和真实工具工作 → Working；权限请求和 `request_user_input` → Waiting；结构化工具失败 → Error；无锁存 Error 的 Stop → Completed；SessionStart → Idle，但 compact 不重置状态。
- Error 保持到下一次普通 prompt 或真实工具工作；SubagentStart/Stop 分别增加和移除同 Workspace 的 Working 成员。
- `$pomodoro-select` 是受支持的 skill。其 prompt hook 先确保当前来源已 report，再发 `select`；该控制 turn 不产生 Working/Completed，只由选择本身开启一个 revision。
- 只有 `$pomodoro-select` 控制事件在 socket 无监听者时 detached 启动插件内 broker；并发启动继续由 Unix socket 排他监听收敛为单实例。普通 lifecycle hook 不启动 broker。
- 自动实例从启动起按十秒空闲期限运行，只有严格校验成功的 `report` 刷新期限；`select`、`remove`、无效请求和串口 `OC1` 心跳均不刷新。到期后关闭串口、客户端 socket 和监听 socket。手工前台入口不启用空闲退出。

## USB and reservation

- 端口由 `POMODORO_PORT` 指定，或仅在恰好存在一个 `/dev/cu.usbmodem*` 时自动发现；缺失、占用或断线均按有界间隔重试。
- 设备发送 `OC1 ENABLED` 后 broker 立即发布，并每三秒发送 `OC1 STATE <revision> <mask>`；`OC1 DISABLED` 停止发布。
- 只接受 revision 等于当前发布 revision、且 mask 是当前原始 mask 中 Error 或 Completed 位的 `OC1 ACK`。ACK 不改写 broker 的原始 mask 或 revision，设备继续在本地清除当前周期的 Error/Completed。
- 有效的用户级 `flash-reservation` 存在时关闭串口；无效或已退出的 PID 不阻塞。保留旧协调文件，不迁移也不删除。
- `SIGINT`/`SIGTERM` 关闭串口与 socket，但保留 selection 与 revision。

## Out of scope

- LaunchAgent、PID 文件、安装到 `~/.local/bin`，以及 start/stop/status 子命令。
- 改写 `/Users/hugo/.config/opencode`；OpenCode adapter 仍是后续 issue。
- 网络服务、新 npm 依赖、非 macOS 串口工作流、固件或 `OC1` 协议变更。
- Codex cloud、远程执行、IDE 手工验收和原生裸 slash command。

## Acceptance

- Node assert 自检覆盖严格校验、租约/PID 清理、来源隔离、聚合、选源、无自动回退、恢复与 revision。
- 临时状态目录和真实 Unix socket 覆盖权限、单实例、陈旧 socket、原子持久化、重启、损坏状态与多连接。
- 可控串口接缝覆盖 `ENABLED`/`DISABLED`、三秒心跳、ACK、占用重试、断线、reservation 与释放。
- `tools/check-flash-reservation.sh` 继续通过。
- Codex 插件自检覆盖 Workspace 归一化、严格 hook 输入、祖先进程发现、状态映射、Error 锁存、控制 turn、subagent 聚合、租约、选源与 broker 重启恢复。
- 自检覆盖选择时按需启动、并发单实例、复用现有 broker，以及仅由有效 `report` 续命的十秒空闲退出；前台入口仍只响应信号退出。
