# 修复时间设置空白并移除 UTC offset

Status: ready-for-human

Settings 底部的 `Date & time` 打开后必须滚回时间编辑器顶部，显示 Year、Month、Day、Hour、Minute 五个滚轮与 Confirm。RTC 直接保存设备本地时间，界面、设置、持久化和统计模型均不包含 UTC offset；运行中的计时在校时后保持剩余时长，Statistics Day 仍按本地午夜划分。

旧开发持久化格式按空存储安全重置。自动时区、夏令时规则和历史统计迁移不在范围内。
