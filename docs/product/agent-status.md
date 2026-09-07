# Agent Status

## Agent Source

一个 Agent Source 是一个宿主限定的 Workspace：Codex Workspace 或 OpenCode Workspace。Git 仓库以规范化仓库根目录为边界；非 Git 目录以规范化工作目录为边界。同一 Workspace 的 chat、fork、side chat 和 subagent 聚合在一起。宿主类型是 identity 的一部分，相同路径的 Codex 与 OpenCode Workspace 不合并。

当前已交付 Codex 与 OpenCode 两个宿主集成。Codex 通过 `$pomodoro-select` 选择；OpenCode 通过 TUI 中的直接 `/pomodoro-select` 动作选择。OpenCode 的 server 与 TUI 插件以用户级方式安装；普通 OpenCode 活动不能发布状态成为首个来源，也不会被自动选择。第一个有效 Codex 来源自动成为 Selected Agent Source；OpenCode 上报不能成为第一个来源。之后只有显式选择（`$pomodoro-select` 或 `/pomodoro-select`）才会切换。所选 Workspace 暂时消失时继续保留选择，等待它恢复，不自动换到其他来源。

首版只支持从 OpenCode TUI 选择 OpenCode Workspace。OpenCode Desktop、Web、非交互 CLI 以及 npm 发布不在首版范围内。

## 状态显示

成员状态按 Error、Waiting、Working、Completed、Idle 的优先级聚合。Main 左下角显示结果；Error 和 Completed 可以确认，Completed 五秒后自动回到 Idle。十秒没有有效心跳时隐藏。

有意义的外部状态变化可以唤醒显示；重复心跳、本地确认、Completed 到期和断开不会反复唤醒。Agent Status 不改变 Mode、Pomodoro、Set Progress、Statistics 或倒计时。

Settings 中的 `Agent status` 默认开启并持久化。关闭后隐藏并忽略状态；重新开启时等待新同步，不显示旧数据。

## Broker 与宿主插件

独立 Node broker 负责来源选择、成员租约、聚合、revision、USB 串口和烧录协调。宿主适配器只通过本地 Unix socket 上报，不直接占用串口。broker 不可用时，宿主工作不被阻塞。

`report`、`select`、`remove` 请求接受可选且严格校验的 `host` 字段，仅接受 `codex`（缺省）与 `opencode`；未知、缺失、多余、畸形、超限或非法字段仍只拒绝该请求。请求只清理自身来源的过期成员，所选来源的过期清理与对应的 revision 推进由后台定时 reap 完成。所选来源持久化为 `{selected, selectedHost, revision}`；旧的只含 `selected` 的状态文件按 Codex 选择加载并可恢复。响应保持既有形状：`selected` 始终是裸 groupId（未选择时为 null），仅当所选来源是 OpenCode 时额外包含 `selectedHost: "opencode"`。

成员每三秒续租，十秒过期。普通工作映射为 Working，等待权限或用户输入映射为 Waiting，结构化工具失败锁存为 Error，成功结束映射为 Completed。压缩上下文不重置状态，新工作会清除 Error。OpenCode 的映射固定为：busy/retry 为 Working（busy 清除本成员已锁存的 Error，retry 不清除），权限或 Question 等待为 Waiting，应答后回到 Working，Session 错误与结构化工具失败锁存为 Error，成功 idle 为 Completed，新建或无活动成员为 Idle；Workspace 聚合按位并后由既有优先级解析。

只有选择操作会在需要时启动 detached broker。自动启动的 broker 在十秒没有有效 Agent report 后退出；手动以前台方式启动的 broker 一直运行到被中断。

## OpenCode 插件安装与卸载

OpenCode 集成由用户级 server 插件（lifecycle 上报）与用户级 TUI 插件（`/pomodoro-select`）组成，二者共享同一 adapter core。安装、校验与卸载使用仓库内脚本：

```sh
node plugins/pomodoro-agent-status/scripts/install-opencode.mjs install   # 或加 --config-dir 指定目录
node plugins/pomodoro-agent-status/scripts/install-opencode.mjs verify    # 含 --project-dir 检查项目级重复
node plugins/pomodoro-agent-status/scripts/install-opencode.mjs uninstall
```

安装会把权威源码复制到用户配置目录并分别在 `opencode.json` 与 `tui.json` 注册；重复执行是幂等的。配置变更只在完全退出所有 OpenCode 进程并重启后生效。最低已验证版本为 OpenCode 1.18.29；安装与升级校验会解析版本并在低于该版本时显式失败。TUI 插件在启动时校验直接 slash 注册 API（`api.command.register`）可用；不可用时安装校验显式失败，绝不注册同名 Markdown prompt 命令或任何会触发模型的回退。`/pomodoro-select` 的反馈只有三种有界结果：选择成功（含 revision）、等待 broker 接管（按需启动 broker 时）、失败（保留原 Selected Agent Source）；反馈不包含 prompt、代码、回复或错误详情。

禁用而不卸载：在 `tui.json` 中将 `plugin_enabled["pomodoro-agent-status"]` 设为 `false` 并从 `opencode.json` 的 `plugin` 数组移除 server 条目，然后完全重启 OpenCode。卸载脚本会同时移除两个条目与安装目录，不影响 broker 已持久化的选择状态。

## USB 与烧录

broker 是唯一 USB 串口所有者，通过有界 `OC1` 行协议与设备通信。该通道只传状态，不传 prompt、代码、响应、错误详情、Pomodoro 命令、设置、统计或固件。

烧录前，`tools/flash.sh` 创建带存活 PID 的 `flash-reservation`。broker 释放串口并在保留期间不重连；成功、失败、超时或中断都会清理自己的保留。脚本不会停止或终止 Codex。

历史 OpenCode Process 集成不是 Agent Source，也不会恢复。当前交付的 OpenCode Workspace 集成是新的 broker-based 实现：server 插件上报、TUI 插件选择、broker 独占串口。旧 OpenCode 状态目录不自动迁移或删除；确认旧 broker 已停止后才可手动清理。
