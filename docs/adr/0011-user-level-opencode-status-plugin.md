# 用户级 OpenCode Status 插件与所选进程协调

OpenCode Status 插件从项目级 `.opencode/plugins/` 迁至用户配置仓库，作为用户级插件在每一个 OpenCode 进程中加载；多个进程通过“显式选择 + 自动回退”协调出唯一的状态来源（Selected OpenCode Process），只有它占用 USB 串口并向设备发布带全局 revision 的聚合状态。USB 烧录保留也从项目目录迁至用户级状态目录，使任意项目下选中的进程都能与 `tools/flash.sh` 协调释放串口。

## Considered Options

- 把插件留在 Pomodoro 仓库并在其他项目里手动复制：插件代码随仓库复制，维护成本高，且其他项目的 OpenCode 进程不加载它，状态圆点只在 Pomodoro 项目中有意义。
- 迁移到用户级配置，作为全局插件加载，让所有 OpenCode 进程共享同一份实现，并通过共享的用户级状态目录协调来源选择。

## Consequences

- 插件实现从 Pomodoro 仓库 `.opencode/plugins/` 迁至用户配置仓库 `plugin/opencode-status.js`，纯协调核心为 `plugin/opencode-status-core.mjs`；Pomodoro 仓库不再包含插件文件，避免同一插件被加载两次。
- 所有加载该插件的 OpenCode 进程在共享状态目录 `${XDG_STATE_HOME:-$HOME/.local/state}/opencode/pomodoro-status/` 下注册实例（随机实例 ID、PID、启动时间、心跳），第一个成功注册的实例自动成为所选进程，后续实例不抢占。
- 在任一进程中执行 `/pomodoro-select`（TUI 插件命令，`tui.json` 加载 `plugin/pomodoro-select.tsx`，不创建 Task、会话消息或模型请求）直接以当前进程 PID 为目标原子地把所选进程切换为当前进程，并总是开启新的状态周期：分配新的全局 revision、清除旧来源的本地确认并唤醒设备，即使聚合颜色未变化。选择结果通过 TUI Toast 反馈；服务器插件不再注册 `command.execute.before` hook，全局只保留这一处 `/pomodoro-select` 注册。
- 所选进程心跳超时或退出后，其余实例按启动时间回退到最早的存活实例；没有存活实例时断开等待。短暂存活的辅助进程不引入额外启发式，回退目标始终是仍在运行的最早进程。
- 全局 revision 在共享状态目录中单调递增、跨进程共享；来源切换与所选进程的聚合状态变化都会递增，心跳保持同一 revision，ACK 只作用于当前已发布 revision。
- 只有所选进程打开 USB 串口并发送三秒心跳；非所选进程持续聚合事件但不发送。存储不可用（目录无法创建、写失败或锁超时）时插件 fail closed，不连接、不竞选，恢复后自动重连。
- 保留文件迁至共享状态目录 `flash-reservation`（写入 PID）；保留期间保留当前 Selected OpenCode Process，但所有插件实例不得竞选、发布状态或打开串口，PID 失效或文件移除后由同一进程恢复连接，仅在它已退出时自动回退；`tools/flash.sh` 的有界等待、统一清理与绝不向 OpenCode 进程发信号的保证不变。
- 此决策取代 ADR 0010 中“项目插件、单进程、项目级保留”的表述；`OC1` 线路协议、设备解析与显示规则不变。
