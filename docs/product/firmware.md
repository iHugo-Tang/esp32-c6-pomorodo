# 固件边界

## 目标平台

固件只面向 Waveshare ESP32-C6-Touch-AMOLED-2.16：480×480 SH8601 QSPI AMOLED、CST9217 触摸、PCF85063 RTC、ES8311 音频、AXP2101 电源管理、16 MB Flash、512 KB HP SRAM，无 PSRAM。

使用 ESP-IDF `>=5.5.2,<5.6`、锁定版本的 LVGL 9 和 `esp_lvgl_port`。显示只使用小型局部缓冲和约 19.2 KB 的旋转缓冲，不分配完整 RGB565 帧缓冲或全屏双缓冲。

UI 以 C 和 LVGL 实现，不使用 SwiftUI。代码检查、结构调整和新增界面应依据 ESP-IDF/LVGL API、现有 Device View 分层及本文件的 UI 文件增长规则；除非仓库明确加入 SwiftUI 源码，否则不应用 SwiftUI 规范。

## 分层

Pomodoro 领域状态、应用编排和设备适配分开。页面不直接读写存储。应用接收用户操作和时间，输出可观察页面状态以及存储、显示、声音和电源效果；平台错误必须可诊断。

公开 Application 接口只包含 `create`、`dispatch`、批量 `adjust_manual_time`、`handle_touch` 和 `destroy`。不要为测试增加专用公开接口或通用命令框架。

## 持久化与更新

Settings 和顶层状态都有格式版本。当前状态通过双槽 journal 保存 Settings、Timer、Set Progress 和 Statistics，断电写入失败时仍能恢复上一份有效数据。

正常 USB 更新保留用户数据；只有明确执行整片擦除才清空。发布后的每个新格式都要支持从上一受支持版本迁移。开发期明确放弃的旧格式可以按空存储安全重置，但不能把这一规则用于已发布数据。

## UI 文件增长

新页面先在现有 Device View 文件中建立清楚的 `create_<page>()` 和 `render_<page>()`。只有出现独立生命周期、资源所有权、反复跨页面泄漏或实际测试边界时，才拆成新模块。

不要因为文件行数或对象数量而提前引入页面注册表、工厂、虚派发、handler table、假 LVGL 或渲染框架。
