# Main Sound Button

Status: done

## Problem Statement

用户无法在主计时界面直接确认声音是否开启，也不能从主界面快速开关声音；当前只能通过 Settings 或实体音量键操作。

## Solution

在 Main 右下角增加固定的 48×48 px Sound 按钮。按钮以扬声器或静音图标表示当前声音状态，点击后切换声音、持久化设置，并显示现有的两秒 Volume overlay。

## User Stories

1. 作为用户，我希望在 Main 一眼看到声音状态，以便确认完成提示是否会播放。
2. 作为用户，我希望直接从 Main 开关声音，以免进入 Settings。
3. 作为用户，我希望按钮在 Ready、Running、Paused 和 Completion Transition 中保持可见，以便随时确认状态。
4. 作为用户，我希望开启声音时看到扬声器图标，静音时看到静音图标。
5. 作为用户，我希望关闭声音后再次开启能恢复上次非零音量。
6. 作为用户，我希望点击 Sound 后看到当前 0–5 音量反馈。
7. 作为用户，我希望音量反馈自动消失且不暂停或干扰计时。
8. 作为用户，我希望 Main、Settings 和实体音量键始终反映同一声音状态。
9. 作为用户，我希望声音设置在重启后仍然保留。
10. 作为用户，我希望自动开始倒计时期间不会误触 Sound。
11. 作为用户，我希望确认对话框出现时，背景 Sound 按钮不能被点击。
12. 作为用户，我希望按钮在四种 UI Orientation 下都保持正确位置和触控行为。
13. 作为用户，我希望按钮足够大，能在设备屏幕上可靠触控。
14. 作为用户，我希望声音关闭后，计时完成的视觉反馈仍正常显示。

## Implementation Decisions

- Sound 按钮位于 Main 右下角，尺寸为 48×48 px，并遵守至少 24 px 的安全边距。
- 使用 LVGL 原生音量与静音符号，不引入图片资源或新依赖。
- `volume > 0` 表示声音开启，`volume == 0` 表示静音。
- 复用现有 Sound toggle action、最后非零音量和持久化路径，不新增声音状态模型。
- 点击按钮后显示现有 StatusOverlay 中的 Volume surface，生命周期为两秒。
- Volume surface 显示图标、五段音量条和 `0/5` 至 `5/5` 数值。
- 按钮在 Ready、Running、Paused 和 Completion Transition 中显示。
- 三秒自动开始倒计时期间，按钮保持可见但处于禁用状态。
- 普通、非自动开始的 Completion Transition 允许切换声音。
- 模态确认遮罩阻止按钮交互。
- 按钮在非 Main 页面和平台错误状态隐藏。
- Settings Sound switch、Main Sound 按钮和实体音量键共享同一状态。
- 更新产品及原型文档，将 Sound 从顶部栏移至 Main 右下角。
- 更新 Volume overlay 规则，使触屏 Sound toggle 与实体音量键均可触发。
- 不新增领域术语或 ADR。

## Testing Decisions

- 最高自动化测试缝为现有 application dispatch seam。
- 扩展已有声音切换测试，验证：
  - 开启和静音状态切换；
  - 最后非零音量恢复；
  - 设置持久化及重启恢复；
  - 点击切换后 Volume overlay 立即可见；
  - 两秒后 Volume overlay 消失；
  - 切换不改变当前计时状态。
- 测试外部可观察行为，不断言内部辅助函数或 LVGL 对象结构。
- 不为这个按钮新增 LVGL UI 测试框架。
- 使用目标设备检查按钮位置、尺寸、图标、各 Main 状态、自动开始禁用、模态遮挡、四向旋转和触控。
- 真机检查确认 Settings、实体按键及 Main 的状态同步。

## Out of Scope

- 改变音量范围或默认音量。
- 新增点击音、导航音或警告音。
- 改变 Focus 和 Break 的完成音效。
- 在其他页面增加 Sound 快捷按钮。
- 重构 StatusOverlay、Settings 或音频驱动。
- 新增独立布尔型静音状态。
- 改变计时、自动开始或 Completion Transition 规则。

## Further Notes

现有系统已经具备声音切换、最后非零音量恢复、持久化、Settings 同步及 Volume overlay。本功能应以最小改动提供新的 Main 入口。
