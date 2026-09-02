---
status: accepted
---

# 使用 ESP-IDF 与 LVGL 构建设备应用

固件采用 ESP-IDF `>=5.5.2,<5.6`，允许使用兼容的 5.5.x 补丁版本；界面采用 Waveshare 官方示例验证过并锁定具体版本的 LVGL 9，通过 Espressif 的 `esp_lvgl_port` 集成显示、触摸与平台节能能力。相比 Arduino 封装层，这条路径提供更直接的驱动、任务和持久化控制；LVGL、`esp_lvgl_port` 与 Waveshare 驱动仍须精确锁定，不得自动追踪最新版。

## Consequences

UI 组件、计时领域状态机和持久化逻辑需要保持分层，页面不得直接读写存储；显示与触摸驱动优先通过 Espressif BSP 或 `esp_lvgl_port` 适配。

仓库中的 UI 工作默认按 C、LVGL 9 与 ESP-IDF 的约束检查和实现。SwiftUI 不属于本项目技术栈；除非仓库明确引入 SwiftUI 源码，否则代理不得加载或套用 SwiftUI 规范来确认 UI 结构。
