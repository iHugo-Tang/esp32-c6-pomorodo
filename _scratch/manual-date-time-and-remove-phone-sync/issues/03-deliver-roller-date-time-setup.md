# 03 — 交付滚轮式日期时间设置

**What to build:** RTC 无效时直接进入不可跳过的手动设置；用户通过年月日、24 小时时分及 UTC offset 滚轮编辑。默认使用固件编译日期和 `12:00`，日期自动收敛；Settings 合并为单一 `Date & time` 行，Confirm 保存、Back 丢弃。

**Blocked by:** 02 — 将本地时间收敛为固定 UTC offset

**Status:** complete

- [x] RTC 无效启动后直接显示不可跳过的滚轮式手动设置。
- [x] 年月日、24 小时时分和 UTC offset 均可通过滚轮编辑。
- [x] 日期范围为 2020–2069，offset 范围为 UTC−12:00 至 UTC+14:00，步进十五分钟。
- [x] RTC 无效时默认日期使用固件编译日期，时间为 `12:00`，offset 使用已保存值或默认值。
- [x] 年月变化导致日期无效时，日期自动收敛到当月最后一天。
- [x] Confirm 校验并写入 RTC、重基准运行中计时、持久化设置；Back 丢弃草稿。
- [x] 首次设置不显示 Back；Settings 入口合并为单一 `Date & time` 行并显示当前本地日期时间及固定 offset。
- [x] 首次设置和 Settings 来源的行为、边界日期及编译日期 fallback 有自动化测试覆盖。
