# RTC 保存设备本地时间

带后备电源的 PCF85063 RTC 直接保存设备本地 wall time，用户只设置年月日与 24 小时时分；设备不再保存或要求输入 UTC offset。产品离线运行且不提供自动时区或夏令时规则，因此本地时间能以最少配置正确划分 Statistics Day；需要校准或调整夏令时时由用户重新设置本地时间。

## Consequences

时间设置固定为 Year、Month、Day、Hour、Minute 五个滚轮。运行中的计时在校时后保持原剩余时长，未来完成记录按修正后的本地日期归档。持久化格式升级，旧开发格式按空存储安全重置。

此决策取代 [ADR-0008](./0008-manual-date-time-and-fixed-utc-offset.md)。
