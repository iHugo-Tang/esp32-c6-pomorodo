# Pomodoro 与统计

## Timer

设备有 Focus、Short break 和 Long break 三种 Mode。每个 Mode 都可能处于 Ready、Running、Paused 或 Completion Transition。

开始计时默认需要用户操作；开启自动开始后，Completion Transition 结束会自动进入下一次计时。Focus 的过渡时间默认 10 秒，可设置为 1–60 秒；休息固定为 3 秒。过渡期间可以立即开始或取消自动开始。

切换 Mode 或 Reset 未完成的 Timer 前必须确认。确认后得到 Aborted Timer：不增加 Pomodoro，也不推进 Set Progress。

## Pomodoro Set

只有自然完成的 Focus 才算一个 Pomodoro。前三个 Pomodoro 后进入 Short break，第四个后进入 Long break。

第四个完成后，Set Progress 在 Long break 期间保持 `4/4`。Long break 完成后重置为 `0/4`；手动选择并完成 Long break 也一样。若跳过 Long break，直到下一次 Focus 开始才重置。

Pomodoro Set 可以跨越 Statistics Day。日期变化不会重置它。Menu 中的 `Clear Set Progress` 需要确认，只清除 Set Progress，不修改当前 Timer 或 Statistics。

## 断电恢复

Running Timer 保存预计结束时间；重启后按 RTC 截止时间恢复。如果关机期间已经到点，只自然结算一次，不自动串行启动下一种 Mode。

Paused Timer 保存剩余时间，关机时间不参与倒计时。持久化数据必须区分“尚未结算”和“已经结算”，避免重复计数。

## Statistics

Statistics 显示今日 Pomodoro、Completed Focus Duration、Daily Goal 进度和 Trailing Seven Days。

完整 Focus 按自然完成时刻归入对应 Statistics Day，跨午夜不拆分。未完成或中止的 Focus 不计入。Daily Goal 与 Pomodoro Set 相互独立，达到目标不会停止或重置 Pomodoro Set。

设备保留最近 365 个 Statistics Day；写入第 366 天时删除最早一天。
