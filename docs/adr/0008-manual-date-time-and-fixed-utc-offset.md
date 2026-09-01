---
status: superseded by ADR-0009
---

# 仅手动设置日期时间并固定 UTC offset

> 已被 [ADR-0009](./0009-store-local-wall-time-in-rtc.md) 取代。

带后备电源的 PCF85063 RTC 是日常运行与离线统计的权威时钟。设备只提供手动日期时间设置与固定 UTC offset；不再提供手机同步、临时 SoftAP、二维码或自动时区规则。

## Considered Options

- 保留手机同步并维护 IANA 时区切换表，需要继续承担 Wi‑Fi、HTTP、二维码与网页流程的复杂度与故障面。
- 让设备连接家庭 Wi‑Fi 并访问 NTP，会引入网络凭据、DNS 与互联网失败处理，不符合无后台联网的产品定位。
- 仅使用手动日期时间与固定 UTC offset，是最简单且符合产品需求的方案。

## Consequences

RTC 保存 UTC；本地时间与 Statistics Day 只由固定的 UTC offset（范围 UTC−12:00 至 UTC+14:00，步进十五分钟）决定。RTC 无效时直接进入不可跳过的手动滚轮设置界面，默认日期为固件编译日期、时间为 `12:00`、offset 为已保存值或默认值。Settings 提供单一的 `Date & time` 入口。固件删除 Wi‑Fi、HTTP、二维码与相关网络依赖，降低构建体积与故障面；不保留旧持久化格式，旧数据按空存储重置。

此决策取代 [ADR-0006](./0006-sync-rtc-from-iphone-over-temporary-softap.md)。
