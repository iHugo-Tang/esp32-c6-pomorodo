# 声音

## 音量

音量范围是 `0–5`，`0` 表示静音。关闭声音时记住最后一个非零音量，再次打开时恢复它。Settings、Main 的 Sound 按钮和物理音量键共享同一状态并持久化。

`KEY/+` 和 `BOOT/-` 在正常运行时调节音量，不改变 BOOT 的上电下载模式。每次调节显示 Volume feedback；最后一次操作两秒后消失。

## 完成音

Focus 和休息自然完成时播放内置 `rest` 音效。静音只关闭声音，不隐藏 Completion Transition，也不阻止显示唤醒。

## Waiting Sound

Settings 中的 `Waiting sound` 默认开启、独立保存，并服从全局 Sound 和当前音量。Agent Status 关闭时仍可编辑，但不会播放。

有效 Agent Status 首次成为 Waiting、从其他状态进入 Waiting、重连后恢复为 Waiting，或确认 Error 后显露 Waiting 时，播放一次内置 `warning` 音效。

持续 Waiting、重复心跳、相同状态的新 revision、已 Waiting 时打开开关、解除静音或提高音量都不补播。音频忙时直接丢弃，不排队、不重试，也不产生 Audio Error。Focus 和休息完成音始终优先。
