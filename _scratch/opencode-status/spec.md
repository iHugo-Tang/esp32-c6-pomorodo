# OpenCode Status Indicator

> **部分被 [`_scratch/shared-opencode-status-plugin/spec.md`](../shared-opencode-status-plugin/spec.md) 取代**：插件已迁至用户级配置并在所有 OpenCode 进程中加载，多个进程通过显式选择 + 自动回退协调出唯一的状态来源，设备只显示所选进程的聚合状态，烧录保留也迁至用户级共享状态目录。本文档中“只汇总单个 OpenCode 进程、不合并多个独立进程”的表述仅适用于所选进程的来源；`OC1` 线路协议、设备解析与显示规则不变。

Status: ready-for-agent

## Problem Statement

设备无法显示当前项目中 OpenCode 的工作状态。用户离开电脑屏幕后，不能从 Pomodoro 主屏判断 OpenCode 正在工作、等待输入、已经完成或发生错误。

## Solution

在 Main 左下角增加一个可确认的状态圆，通过 USB Serial/JTAG 与当前项目的 OpenCode 插件双向通信。设备汇总一个 OpenCode 进程内的全部项目会话，以五种颜色显示 Error、Waiting、Working、Completed 与 Idle，并由默认开启且持久化的 `OpenCode status` 设置控制整个集成。

## User Stories

1. 作为用户，我希望从设备颜色判断 OpenCode 当前是否需要关注。
2. 作为用户，我希望任一会话等待问题回答或权限确认时看到黄色。
3. 作为用户，我希望 OpenCode 工作时看到蓝紫色，完成时看到绿色，空闲时看到灰色，出错时看到红色。
4. 作为用户，我希望后台会话的 Error 或 Waiting 不被另一个 Idle 会话覆盖。
5. 作为用户，我希望状态变化唤醒屏幕，包括我用 PWR 主动熄屏之后。
6. 作为用户，我希望完成提示五秒后自动回到 Idle，也能点击提前确认。
7. 作为用户，我希望错误保持可见，直到我点击确认、会话重新工作或 OpenCode 断开。
8. 作为用户，我希望圆点不显示会话标题、提示词、代码、回复或错误详情。
9. 作为用户，我希望关闭设置后设备隐藏圆点、不读取 OpenCode 状态，也不因 OpenCode 唤醒。
10. 作为用户，我希望重新插拔设备或暂时烧录固件后，状态通信能自动恢复而不打断 OpenCode。

## Status Model

| Status | Color | Meaning | Lifetime |
| --- | --- | --- | --- |
| Error | `#FF5C5C` | 至少一个会话发生未确认错误 | 保持到确认、重新工作或断开 |
| Waiting | `#FFD166` | 至少一个会话等待问题回答或权限决定 | 等待事件被回复或拒绝 |
| Working | `#5B8CFF` | 至少一个会话 busy 或 retrying | 会话离开进行中状态 |
| Completed | `#4DD17A` | 至少一个会话刚完成 | 五秒或点击确认 |
| Idle | `#454B56` | 已连接且没有更高优先级状态 | 直到状态变化或断开 |

多会话优先级固定为 `Error > Waiting > Working > Completed > Idle`。确认 Error 或 Completed 后，设备显示当前仍存在的下一优先级状态。

## UI Decisions

- 圆点直径 28 px，使用 Main 左下角 `LV_ALIGN_BOTTOM_LEFT`，距左边 26 px，并与声音按钮竖直居中对齐。
- 使用 44×44 px 透明触控区；只有 Error 与 Completed 响应点击，且点击仅确认，不打开详情。
- 只在 Main 显示；子页面、平台错误状态和阻断 Dialog 不显示或遮挡它。
- 圆点加入现有像素偏移对象，随 UI Orientation 与触摸坐标一起旋转。
- 有效的外部聚合状态变化唤醒屏幕。重复心跳、本地确认、Completed 到期与断线隐藏不重复唤醒。
- 未连接、十秒心跳超时或设置关闭时隐藏圆点。

## Settings Decisions

- Settings 增加 `OpenCode` 区块与 `OpenCode status` Switch，默认 On，切换后立即持久化。
- 从旧持久化版本迁移时使用 On，不改变其他 Settings、Timer 或 Statistics 数据。
- Off 隐藏圆点、停止读取 OpenCode 状态并禁止相关唤醒；设备继续周期发送 `DISABLED`，使插件停止写入。
- On 周期发送 `ENABLED`；插件收到后立即同步并恢复三秒心跳。

## OpenCode Event Mapping

- `permission.asked` 与 `question.asked` → Waiting。
- 对应 replied、rejected 事件解除 Waiting；会话仍运行时回到 Working。
- `session.status` 的 busy 与 retry → Working。
- 正常 `session.idle` → Completed；五秒后成为 Idle。
- `session.error` → Error，并锁存到确认、该会话出现新工作或断开。
- `session.deleted` 从聚合中移除该会话。
- 插件只汇总其所在项目和单个 OpenCode 进程；首版不合并多个独立 OpenCode 进程。（该表述已被用户级多进程协调取代：设备只显示所选进程的聚合状态，见 `_scratch/shared-opencode-status-plugin/spec.md`。）

## USB Protocol Decisions

- 使用换行结尾、带 `OC1` 版本前缀的短 ASCII 行；固件拒绝超长、未知版本或格式错误的输入。
- 插件发送 `OC1 STATE <revision> <mask>`。`mask` 同时携带当前存在的 Error、Waiting、Working 与 Completed 位，使确认最高优先级后能显示下一状态；零值表示 Idle。
- `revision` 在项目聚合状态发生变化时递增，三秒心跳重复同一 revision，不触发重复唤醒。
- 设备发送 `OC1 ENABLED`、`OC1 DISABLED` 和 `OC1 ACK <revision> <mask>`。确认只清除对应 revision 中的 Error 或 Completed，不影响新事件。
- 插件在 macOS 自动连接唯一的 `/dev/cu.usbmodem*`；存在多个候选时只使用 `POMODORO_PORT` 指定的路径，不猜测。
- 串口不可用或被刷写/监视工具占用时，插件静默断开并定期重连，不使 OpenCode 任务失败。
- 插件不引入 npm 依赖，不建立额外服务，不发送提示词、代码、回复或会话详情。

## Testing Decisions

- 固件主机测试覆盖协议解析边界、优先级、revision 去重、确认、五秒 Completed 到期、十秒断线、设置关闭和迁移默认值。
- 应用测试验证只有有效外部状态变化触发显示唤醒，重复心跳与本地状态变化不重复唤醒，且不改变 Timer、Pomodoro Set 或 Statistics。
- 插件使用一个无串口硬件依赖的小型测试，覆盖事件映射、多会话聚合、优先级、revision 与确认。
- 不新增 LVGL UI 测试框架；位置、颜色、触控、Dialog 遮挡、像素偏移和四种 UI Orientation 使用目标设备验收。
- 端到端验收覆盖 OpenCode Working、Waiting、Completed、Idle、Error，设置 Off/On、拔插、串口占用和自动重连。

## Out of Scope

- Wi-Fi、BLE、互联网服务、手机中继或云同步。
- 在设备上回答问题、批准权限、查看会话详情或控制 OpenCode。
- 同时合并多个 OpenCode 进程、多个项目或多台电脑。（部分被取代：多进程通过显式选择 + 自动回退协调，但设备仍只显示所选进程的聚合状态，不跨进程合并。）
- Windows 与 Linux 串口发现；首版只支持当前 macOS 工作流。
- 改变 Pomodoro Timer、Pomodoro Set、Statistics 或完成提示规则。
- 将 USB 状态协议扩展为固件更新或通用远程控制协议。
