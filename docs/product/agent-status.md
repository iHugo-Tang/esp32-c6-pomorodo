# Agent Status

## Agent Source

一个 Codex Workspace 是一个 Agent Source。Git 仓库以规范化仓库根目录为边界；非 Git 目录以规范化工作目录为边界。同一 Workspace 的 chat、fork、side chat 和 subagent 聚合在一起。

第一个有效来源自动成为 Selected Agent Source。之后只有显式执行 `$pomodoro-select` 才会切换。所选 Workspace 暂时消失时继续保留选择，等待它恢复，不自动换到其他来源。

## 状态显示

成员状态按 Error、Waiting、Working、Completed、Idle 的优先级聚合。Main 左下角显示结果；Error 和 Completed 可以确认，Completed 五秒后自动回到 Idle。十秒没有有效心跳时隐藏。

有意义的外部状态变化可以唤醒显示；重复心跳、本地确认、Completed 到期和断开不会反复唤醒。Agent Status 不改变 Mode、Pomodoro、Set Progress、Statistics 或倒计时。

Settings 中的 `Agent status` 默认开启并持久化。关闭后隐藏并忽略状态；重新开启时等待新同步，不显示旧数据。

## Broker 与 Codex 插件

独立 Node broker 负责来源选择、成员租约、聚合、revision、USB 串口和烧录协调。Codex 插件只通过本地 Unix socket 上报，不直接占用串口。broker 不可用时，Codex 工作不被阻塞。

成员每三秒续租，十秒过期。普通工作映射为 Working，等待权限或用户输入映射为 Waiting，结构化工具失败锁存为 Error，成功结束映射为 Completed。压缩上下文不重置状态，新工作会清除 Error。

只有选择操作会在需要时启动 detached broker。自动启动的 broker 在十秒没有有效 Agent report 后退出；手动以前台方式启动的 broker 一直运行到被中断。

## USB 与烧录

broker 是唯一 USB 串口所有者，通过有界 `OC1` 行协议与设备通信。该通道只传状态，不传 prompt、代码、响应、错误详情、Pomodoro 命令、设置、统计或固件。

烧录前，`tools/flash.sh` 创建带存活 PID 的 `flash-reservation`。broker 释放串口并在保留期间不重连；成功、失败、超时或中断都会清理自己的保留。脚本不会停止或终止 Codex。

OpenCode Process 不再是 Agent Source。旧 OpenCode 状态目录不自动迁移或删除；确认旧 broker 已停止后才可手动清理。
