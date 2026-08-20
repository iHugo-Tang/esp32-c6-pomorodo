# 01 — 修复空白时间编辑器并使用本地时间

Type: bug

Status: ready-for-human

Blocked by: none

- [x] 从 Settings 打开 `Date & time` 时共享内容滚回顶部。
- [x] 编辑器只显示 Year、Month、Day、Hour、Minute 五个滚轮。
- [x] RTC、应用、统计与持久化不再需要 UTC offset。
- [x] 本地午夜继续决定 Statistics Day，校时保持运行中计时的剩余时长。
- [x] 更新 ADR、产品说明和硬件验收步骤。
- [x] 主机测试与完整 ESP-IDF 构建通过。
- [x] 真机确认五个滚轮可见可操作，设置结果在断电重启后保持。
