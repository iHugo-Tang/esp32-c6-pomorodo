# 通过临时 SoftAP 从 iPhone 同步 RTC

> **Superseded by [ADR-0008](./0008-manual-date-time-and-fixed-utc-offset.md).**
> 设备只使用手动日期时间与固定 UTC offset，不再提供手机同步或自动时区规则。

带后备电源的 PCF85063 RTC 是日常运行与离线统计的权威时钟。需要校时时，设备临时创建带随机密码且五分钟失效的 Wi‑Fi 热点并显示连接二维码；iPhone 打开设备托管的本地页面后，将自身已校准的 UTC 时间、IANA 时区名称及未来五年的 UTC offset 切换表传给设备，用户确认后写入 RTC，随后设备关闭 Wi‑Fi 并删除全部会话数据。

## Considered Options

- 让设备连接家庭 Wi‑Fi 并访问 NTP 会引入家庭网络凭据、DNS 与互联网失败处理。
- Android DPP 二维码不适用于用户现有的 iPhone。
- 在 2.16 英寸屏幕上输入 Wi‑Fi 密码体验较差。

## Consequences

RTC 保存 UTC，设备按同步得到的 offset 切换表计算本地时间与 Statistics Day；规则到期前提示用户重新同步。首次设置同时保留固定 UTC offset 的手动日期时间入口作为失败回退。固件不保存 Wi‑Fi 凭据、不访问互联网，也不在后台启用无线电。
