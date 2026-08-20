# 01 — 移除手机同步流程与无线依赖

**What to build:** 用户不再看到或进入手机同步；固件删除 SoftAP、HTTP、二维码、同步状态机及相关依赖。现有手动设置仍可正常使用，测试与产品文档同步更新，并记录 supersede ADR-0006 的新决策。

**Blocked by:** None — can start immediately

**Status:** complete

- [x] 首次设置和 Settings 不再显示或进入手机同步流程。
- [x] SoftAP、HTTP、二维码、同步状态机及相关平台接口被移除。
- [x] Wi‑Fi、HTTP、网络接口、事件循环和二维码构建依赖被移除。
- [x] 现有手动日期时间设置仍可进入和完成。
- [x] 手机同步相关测试、产品文档和原型导航已删除或更新。
- [x] 新 ADR 记录仅手动设置日期时间的决策，并 supersede ADR-0006。
