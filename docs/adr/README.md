# ADR 索引

ADR 记录重要决定及其原因。编号表示决定发生的顺序，不表示当前有效性；请先看状态，再看正文。

## Pomodoro 与数据

- [ADR-0002：持久化并恢复当前计时](0002-persist-and-recover-active-timer.md) — accepted
- [ADR-0003：保留 365 天每日统计](0003-retain-365-days-of-daily-statistics.md) — accepted
- [ADR-0007：仅通过 USB-C 更新固件](0007-use-usb-only-firmware-updates.md) — accepted

## 日期与时间

- [ADR-0001：使用带后备电源的 RTC](0001-use-battery-backed-rtc-for-offline-statistics.md) — superseded
- [ADR-0006：通过 iPhone SoftAP 同步 RTC](0006-sync-rtc-from-iphone-over-temporary-softap.md) — superseded
- [ADR-0008：手动时间与固定 UTC offset](0008-manual-date-time-and-fixed-utc-offset.md) — superseded
- [ADR-0009：RTC 保存本地 wall time](0009-store-local-wall-time-in-rtc.md) — accepted，当前决定

## 固件与硬件

- [ADR-0004：使用 ESP-IDF 与 LVGL](0004-use-esp-idf-and-lvgl.md) — accepted
- [ADR-0005：固定目标硬件](0005-target-waveshare-esp32-c6-touch-amoled-2-16.md) — accepted

## 产品范围

- [ADR-0017：设备仅作为番茄钟](0017-use-device-only-as-pomodoro-timer.md) — accepted，当前产品边界

## Agent Status（历史）

- [ADR-0010：USB OpenCode Status](0010-use-usb-for-opencode-status.md) — historical，状态协议已移除，仅保留只读截图
- [ADR-0011：用户级 OpenCode 插件](0011-user-level-opencode-status-plugin.md) — superseded
- [ADR-0012：独立 Agent Status broker](0012-use-independent-agent-status-broker.md) — superseded by ADR-0017
- [ADR-0013：Codex Workspace lifecycle adapter](0013-use-codex-workspace-lifecycle-adapter.md) — superseded by ADR-0017
- [ADR-0014：选择时按需启动 broker](0014-start-agent-status-broker-on-selection.md) — superseded by ADR-0017
- [ADR-0015：只支持 Codex Workspace](0015-agent-status-only-supports-codex-workspaces.md) — superseded by ADR-0016
- [ADR-0016：支持 OpenCode Workspace](0016-support-opencode-workspaces-as-agent-sources.md) — superseded by ADR-0017
