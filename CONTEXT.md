# Pomodoro Timer

用于描述设备如何组织专注、休息与每日进度的统一产品语言。

## Language

**Pomodoro**：
一次完整完成的专注计时。只有专注自然倒计时结束才增加番茄数；若运行中的计时跨越断电且预计结束时间已经到达，也视为自然完成。休息或手动切换模式不计入番茄。
_Avoid_：专注模式、计时器、Session

**Pomodoro Set**：
固定包含四个番茄的进度单位；前三个番茄后进入短休息，第四个番茄后进入长休息。任何完整结束的长休息都会结束当前组并开始新的一组，即使它由用户提前手动选择。Pomodoro Set 可以跨越 Statistics Day，不在午夜重置。
_Avoid_：每日目标、轮次、Session

**Set Progress**：
当前番茄组内已完成的番茄数，以 `已完成数/4` 表示，例如 `3/4`。第四个番茄完成后，完成态及长休息期间保持 `4/4`；长休息完成后或用户显式清除时重置为 `0/4`，若用户跳过长休息，则在下一次专注开始时重置。
_Avoid_：每日进度、目标完成度

**Daily Goal**：
用户希望当天完成的番茄总数，是独立于番茄组的统计指标；达到目标不会停止或重置番茄组。
_Avoid_：组进度、每组番茄数

**Statistics Day**：
由设备本地时间 `00:00` 至次日 `00:00` 的统计区间。跨越午夜的番茄及其完整专注时长按自然完成时刻归入对应统计日，不在日期之间拆分。
_Avoid_：过去 24 小时、使用日

**Completed Focus Duration**：
统计日内所有自然完成番茄各自设定时长的总和。暂停时间、中止计时和尚未完成的当前专注均不计入。
_Avoid_：实际使用时长、屏幕开启时长、专注总时长

**Trailing Seven Days**：
包含今日在内、从最早日期到今日排列的连续七个统计日；它是滚动窗口，不是周一至周日的自然周。
_Avoid_：本周、一周

**Pending Statistics**：
RTC 无效期间自然完成、但尚不能归属 Statistics Day 的番茄与专注时长。时间恢复后，它们统一归入恢复时所在的 Statistics Day。
_Avoid_：丢失记录、未完成番茄

**Mode**：
计时器当前承担的活动类型，取值为 Focus、Short break 或 Long break。
_Avoid_：番茄、页面

**Aborted Timer**：
用户通过切换模式或 Reset 确认放弃的未完成计时。中止计时不增加番茄数，也不推进 Pomodoro Set。
_Avoid_：完成、跳过

**Completion Transition**：
一次计时自然结束后、下一模式开始前的反馈阶段。启用自动开始时，Pomodoro 的该阶段默认持续十秒且可在设置中调整为一至六十秒，休息的该阶段固定持续三秒；用户仍可立即开始或取消自动开始。Daily Goal 达成反馈属于同一个 Pomodoro Completion Transition，不是独立完成阶段。
_Avoid_：完成模式、完成页面

**UI Orientation**：
设备依据重力方向采用的四个正交界面方向之一；界面显示与触摸坐标同步旋转，不表示设备倾角或动作手势。
_Avoid_：设备姿态、屏幕旋转、IMU 手势

**USB Power**：
设备正在通过 USB 接收外部电源的状态，与电池是否正在充电以及 USB 串口是否连接无关。USB Power 期间禁用基于电池的显示省电行为。
_Avoid_：Charging、充电状态、USB 连接

**OpenCode Status**：
当前项目内全部 OpenCode 会话的汇总状态，按优先级取 Error、Waiting、Working、Completed 或 Idle；Completed 在确认或短暂展示后回到 Idle。
_Avoid_：Agent 状态、Pomodoro Mode
