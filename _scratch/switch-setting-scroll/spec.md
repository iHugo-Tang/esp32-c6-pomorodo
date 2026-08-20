# Switch Setting Scroll

Status: ready-for-human

## Problem Statement

在 Settings 中从带开关的 row 开始纵向拖动时，row 内容会移动，而不是滚动 Settings 列表。

## Solution

带开关的 Settings row 不可自行滚动，并将纵向拖动交给 Settings 列表。拖动不改变开关状态；轻点 row 或 switch 仍只切换一次。普通 action rows 保持现有行为。

## Testing Decisions

- Host tests 和固件构建验证改动可正常集成。
- 目标设备验证覆盖 row 背景、文字和 switch 三种拖动起点。
- 真机验收仅覆盖默认 UI Orientation。
- 不新增 LVGL UI 测试框架。

## Out of Scope

- 更改 Settings 的视觉布局。
- 更改普通 action rows 的交互。
- 扩大到其他 UI Orientation 的真机验收。
