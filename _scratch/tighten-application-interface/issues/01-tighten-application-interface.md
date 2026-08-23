# 01 — 收紧 Application 公开接口

**What to build:** 让 Application 的公开接口与真实生产边界一致：调用方通过生命周期、dispatch、批量手动时间滚轮和 touch 入口驱动应用；测试也经过相同的用户可达流程，而不依赖 settings、numeric editor、brightness 或 manual-time confirmation 的内部 helper。

**Blocked by:** None — can start immediately.

**Status:** complete

- [x] 公开接口只保留 `create`、`dispatch`、批量 `adjust_manual_time`、`handle_touch` 与 `destroy`，接口说明准确描述主要行为边界及必要例外。
- [x] Settings confirmation、manual-time confirmation 和 numeric editor helper 仅在 Application 实现内部可见。
- [x] 没有生产调用方的 brightness preview/confirm 直达流程被删除，亮度预览、取消恢复和确认保存仍通过 numeric actions 正常工作。
- [x] Manual-time confirmation 只在 `TIME_SETUP` phase 有效；运行中校时经过 Settings、Date & time 和 confirm actions，且保持当前计时剩余量。
- [x] Application tests 使用 dispatch 覆盖 numeric settings、settings 持久化、首次时间设置、运行中校时及重启恢复，不再直接注入 UI 无法产生的 settings 或 manual-time 值。
- [x] 批量手动时间调整仍一次应用多步变化并只渲染最终值；touch 入口仍正确处理首次唤醒 consumed、亮度恢复和失败行为。
- [x] 未新增 command framework、handler table、numeric 批量入口、测试专用 API、条件编译 seam 或 opaque state 访问。
- [x] 全部 native host tests 与完整 ESP-IDF firmware build 通过，用户可观察的 settings、manual time、brightness、touch、显示唤醒和持久化行为不变。

## Comments

- 2026-08-23: Public Application functions reduced to the five agreed seams. Internal confirmation/editor helpers are static, the unused brightness bypass is deleted, and manual-time confirmation now requires `TIME_SETUP`. Tests exercise settings and confirmation through dispatch while retaining direct coverage only for batch roller and touch adapters. Native tests pass 5/5 and the complete ESP-IDF build succeeds.
