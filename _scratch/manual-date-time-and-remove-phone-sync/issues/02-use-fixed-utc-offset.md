# 02 — 将本地时间收敛为固定 UTC offset

> 已由 [`local-date-time`](../../local-date-time/spec.md) 取代；保留此文件记录旧实现。

**What to build:** 删除已不可达的 IANA 时区和自动 offset 切换表；本地时间及 Statistics Day 只使用固定 UTC offset。持久化格式升级，旧格式安全重置为默认状态而非进入 Storage Error。

**Blocked by:** 01 — 移除手机同步流程与无线依赖

**Status:** complete

- [x] 本地时间和 Statistics Day 只使用固定 UTC offset。
- [x] IANA 时区名称、自动 offset 切换表和规则到期提示不再存在。
- [x] 持久化格式版本升级。
- [x] 旧持久化格式被视为空存储并恢复默认状态，不显示 Storage Error。
- [x] RTC 有效时重置后的状态进入 Main；RTC 无效时进入强制手动时间设置。
- [x] 固定 offset 的边界和 Statistics Day 归属有自动化测试覆盖。
