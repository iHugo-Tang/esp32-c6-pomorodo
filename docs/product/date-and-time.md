# 日期与时间

## 本地时间

PCF85063 RTC 直接保存设备本地 wall time。设备不保存 UTC offset，也没有时区、夏令时、手机同步、NTP、Wi‑Fi 或二维码校时。

Statistics Day 从本地 `00:00` 到次日 `00:00`。校正时间后，正在运行的 Timer 保持原剩余时长，之后完成的记录按新日期归档。

RTC 无效期间自然完成的 Focus 进入 Pending Statistics。时间恢复后，这些记录统一归入恢复时所在的 Statistics Day。

## 设置时间

RTC 无效时，设备在进入 Main 前打开不可跳过的日期时间设置。用户设置 Year、Month、Day、Hour 和 Minute，使用 24 小时制；支持年份 2020–2069。

首次草稿使用固件编译日期和 `12:00`。年月变化会自动收敛非法日期。五个滚轮到边界后停止，不循环；快速滑动只在停稳后提交一次结果。

首次设置没有 Back。Settings 中的 `Date & time` 使用同一编辑器：Back 丢弃草稿，Confirm 校验并写入 RTC，然后返回 Settings。
