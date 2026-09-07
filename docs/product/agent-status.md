# Agent Status

## Agent Source

一个 Agent Source 是一个宿主限定的 Workspace：Codex Workspace 或 OpenCode Workspace。Git 仓库以规范化仓库根目录为边界；非 Git 目录以规范化工作目录为边界。同一 Workspace 的 chat、fork、side chat 和 subagent 聚合在一起。宿主类型是 identity 的一部分，相同路径的 Codex 与 OpenCode Workspace 不合并。

当前已实现 Codex 与 OpenCode 两个宿主集成，并有可运行的自动化 host 检查覆盖；真实 OpenCode TUI 冒烟与物理设备验收仍待人工执行，本文不对未发生的验收作声明。Codex 通过 `$pomodoro-select` 选择；OpenCode 通过 TUI 中的直接 `/pomodoro-select` 动作选择。OpenCode 的 server 与 TUI 插件以用户级方式安装；普通 OpenCode 活动不能发布状态成为首个来源，也不会被自动选择。第一个有效 Codex 来源自动成为 Selected Agent Source；OpenCode 上报不能成为第一个来源。之后只有显式选择（`$pomodoro-select` 或 `/pomodoro-select`）才会切换。所选 Workspace 暂时消失时继续保留选择，等待它恢复，不自动换到其他来源。

首版只支持从 OpenCode TUI 选择 OpenCode Workspace。OpenCode Desktop、Web、非交互 CLI 以及 npm 发布不在首版范围内。

## 状态显示

成员状态按 Error、Waiting、Working、Completed、Idle 的优先级聚合。Main 左下角显示结果；Error 和 Completed 可以确认，Completed 五秒后自动回到 Idle。十秒没有有效心跳时隐藏。

有意义的外部状态变化可以唤醒显示；重复心跳、本地确认、Completed 到期和断开不会反复唤醒。Agent Status 不改变 Mode、Pomodoro、Set Progress、Statistics 或倒计时。

Settings 中的 `Agent status` 默认开启并持久化。关闭后隐藏并忽略状态；重新开启时等待新同步，不显示旧数据。

## Broker 与宿主插件

独立 Node broker 负责来源选择、成员租约、聚合、revision、USB 串口和烧录协调。宿主适配器只通过本地 Unix socket 上报，不直接占用串口。broker 不可用时，宿主工作不被阻塞。

`report`、`select`、`remove`、`activate` 请求接受可选且严格校验的 `host` 字段，仅接受 `codex`（缺省）与 `opencode`；未知、缺失、多余、畸形、超限或非法字段仍只拒绝该请求。`activate` 携带与 `report` 相同的成员字段，在一个串行 broker 步骤内刷新激活成员并完成显式选择，因此每次被接受的调用恰好推进一个 revision（首次选择、重复选择、重启后对已选但空置来源的恢复均如此），并发调用各自恰好推进一次；与普通上报不同，`activate` 可以为其宿主建立首次选择。请求只清理自身来源的过期成员，所选来源的过期清理与对应的 revision 推进由后台定时 reap 完成。所选来源持久化为 `{selected, selectedHost, revision}`；旧的只含 `selected` 的状态文件按 Codex 选择加载并可恢复。响应保持既有形状：`selected` 始终是裸 groupId（未选择时为 null），仅当所选来源是 OpenCode 时额外包含 `selectedHost: "opencode"`。

成员每三秒续租，十秒过期。普通工作映射为 Working，等待权限或用户输入映射为 Waiting，结构化工具失败锁存为 Error，成功结束映射为 Completed。压缩上下文不重置状态，新工作会清除 Error。OpenCode 的映射固定为：busy/retry 为 Working（busy 清除本成员已锁存的 Error，retry 不清除），权限等待为 Waiting（1.18.29 实际发布 `permission.updated`/`permission.replied`），Question 等待为 Waiting——1.18.29 的插件事件类型中没有 question 事件，Waiting 通过运行中的 `question` 工具 part 识别，question 工具结束（完成或出错，例如被拒绝）后回到 Working 而不锁存 Error，Session 错误与普通工具的 error part 锁存为 Error，成功 idle 为 Completed，新建或无活动成员为 Idle；Workspace 聚合按位并后由既有优先级解析。插件同时接受后续版本的 `permission.asked`/`question.*` 事件及 `permission.v2.*`/`question.v2.*` 命名别名；这些别名是前向兼容，不作为 1.18.29 行为的验收证据。

只有选择操作会在需要时启动 detached broker。自动启动的 broker 在十秒没有有效 Agent report 后退出；手动以前台方式启动的 broker 一直运行到被中断。

## OpenCode 插件安装与卸载

OpenCode 集成由用户级 server 插件（lifecycle 上报）与用户级 TUI 插件（`/pomodoro-select`）组成，二者共享同一 adapter core。安装、校验与卸载使用仓库内脚本：

```sh
node plugins/pomodoro-agent-status/scripts/install-opencode.mjs install   # 或加 --config-dir 指定目录
node plugins/pomodoro-agent-status/scripts/install-opencode.mjs verify    # 含 --project-dir 检查项目级重复
node plugins/pomodoro-agent-status/scripts/install-opencode.mjs uninstall
```

安装会把权威源码复制到用户配置目录并分别在 `opencode.json` 与 `tui.json` 注册；重复执行是幂等的。安装器只重写纯 JSON 配置：当存在 `opencode.jsonc` 或 `tui.jsonc`（单独存在或与同名 JSON 并存）、或任一配置文件无法解析时，安装、卸载与校验会在复制或写入任何内容之前整体拒绝，保留全部文件字节不变，并给出手动处理指引；两个目标文件都会先通过解析校验再开始写入。按需启动的 broker 子进程使用显式 Node 运行时执行：`POMODORO_BROKER_NODE` 可显式指定；非 Node 插件宿主（编译后的 OpenCode/Bun TUI，其 `process.execPath` 是宿主二进制，无法运行 broker 脚本）会从 PATH 解析 `node`，不可用时报错而不是静默失败；`verify` 会校验该运行时。配置变更只在完全退出所有 OpenCode 进程并重启后生效。最低已验证版本为 OpenCode 1.18.29；安装与升级校验解析版本并在低于该版本时显式失败。直接 slash 注册 API（`api.command.register`）由 TUI 插件在启动时校验：不可用时 TUI 插件启动显式失败，绝不注册同名 Markdown prompt 命令或任何会触发模型的回退；安装校验本身只负责版本下限、Node 运行时与文件/注册项一致性。`verify --project-dir` 会同时检查 `.opencode/plugin/` 与 `.opencode/plugins/` 两个项目级目录，确认没有重复的 Pomodoro 插件副本。`/pomodoro-select` 的反馈只有三种有界结果：选择成功（含 revision）、等待 broker 接管（按需启动 broker 时）、失败（保留原 Selected Agent Source）；反馈不包含 prompt、代码、回复或错误详情。

禁用而不卸载：在 `tui.json` 中将 `plugin_enabled["pomodoro-agent-status"]` 设为 `false` 并从 `opencode.json` 的 `plugin` 数组移除 server 条目，然后完全重启 OpenCode。卸载脚本会同时移除两个条目与安装目录，不影响 broker 已持久化的选择状态。

## USB 与烧录

broker 是唯一 USB 串口所有者，通过有界 `OC1` 行协议与设备通信。该通道只传状态，不传 prompt、代码、响应、错误详情、Pomodoro 命令、设置、统计或固件。

烧录前，`tools/flash.sh` 创建带存活 PID 的 `flash-reservation`。broker 释放串口并在保留期间不重连；成功、失败、超时或中断都会清理自己的保留。脚本不会停止或终止 Codex。

历史 OpenCode Process 集成不是 Agent Source，也不会恢复。当前交付的 OpenCode Workspace 集成是新的 broker-based 实现：server 插件上报、TUI 插件选择、broker 独占串口。旧 OpenCode 状态目录不自动迁移或删除；确认旧 broker 已停止后才可手动清理。
