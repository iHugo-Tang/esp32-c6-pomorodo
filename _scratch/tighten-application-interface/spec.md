# 收紧 Application interface

Status: complete

## Problem Statement

`pomodoro_application.h` 把 Application 行为边界描述为 `create + dispatch`，却又公开多组只被测试直接调用的 settings、numeric editor、brightness 和 manual-time helper。这些入口允许测试绕过真实页面与 action 流程，并让内部实现细节看起来像受支持的 Application API。

生产平台在 `dispatch` 之外确实需要两个输入入口：手动时间滚轮一次提交多步变化，以避免快速滚动时重复渲染；touch 入口需要返回首次唤醒是否消费本次触摸。`destroy` 仍是 `create` 分配对象的必要生命周期出口。

## Solution

Application 的公开接口只保留 `create`、`dispatch`、批量 `adjust_manual_time`、`handle_touch` 与 `destroy`。settings confirmation、manual-time confirmation 和 numeric editor 操作成为实现文件内部函数；没有生产路径的 brightness preview/confirm 直达函数删除。

Application 测试通过 `dispatch` 驱动真实 action 与页面状态。只有批量手动时间调整和 touch 行为直接调用其保留的生产入口。这个改动不引入 command framework、handler table、测试专用 API 或条件编译 seam。

## User Stories

1. 作为固件维护者，我希望头文件只暴露生产代码真正使用的 Application interface，以便调用方能够区分稳定边界与内部 helper。
2. 作为测试维护者，我希望 Application 测试经过 `dispatch`，以便验证与真实 UI 相同的状态约束和副作用。
3. 作为设备用户，我希望快速滚动日期时间控件仍然一次应用多步变化并只渲染最终值。
4. 作为设备用户，我希望熄屏后的首次触摸仍然只唤醒显示并被消费。
5. 作为固件维护者，我希望删除不可达的 brightness 直达流程，以免出现两套设置语义。

## Interface Decisions

- `pomodoro_application.h` 公开：
  - `pomodoro_application_create`
  - `pomodoro_application_dispatch`
  - `pomodoro_application_adjust_manual_time`
  - `pomodoro_application_handle_touch`
  - `pomodoro_application_destroy`
- 头文件注释把 `dispatch` 定义为主要行为边界，并明确批量滚轮、touch 与生命周期出口是必要的 adapter/lifecycle 入口。
- `confirm_settings`、`confirm_manual_time`、`open_numeric_setting`、`adjust_numeric_setting`、`confirm_numeric_setting` 只在 `pomodoro_application.c` 内可见。
- `preview_brightness` 与 `confirm_brightness` 删除；亮度预览、取消恢复和确认保存只走 numeric action 流程。
- 内部 manual-time confirmation 只接受 `TIME_SETUP` phase。运行中校时必须先 dispatch `OPEN_SETTINGS` 与 `OPEN_DATE_TIME`。
- 不改变 action enum、view model、platform callbacks 或设备页面行为。

## Testing Decisions

- 把 numeric open、increment/decrement 与 confirm 的直接 helper 调用改为对应 dispatch actions。
- 把 settings 持久化测试改为通过 UI 可达的 action 分别修改设置，再重启验证恢复；不再承诺整包 settings 原子替换。
- 把首次时间设置与运行中校时测试改为进入 `TIME_SETUP`、调整 draft，再 dispatch `MANUAL_TIME_CONFIRM`。
- 日期范围、闰日与滚轮收敛由现有 date/manual editor seam 覆盖；删除通过已隐藏 confirmation helper 注入 UI 不可达结构的 Application 测试。
- 测试可以直接调用保留的批量 `adjust_manual_time` 来高效准备 draft，但确认动作必须经过 dispatch。
- touch 测试继续直接调用 `handle_touch`，验证 consumed、唤醒、亮度恢复与失败行为。
- 保留批量滚轮只渲染一次的回归测试。
- 运行全部 native host tests 与完整 ESP-IDF build。

## Documentation Decisions

- 更新 Application header 注释，使声明与真实公开接口一致。
- 这是可逆的实现 seam，不是 Pomodoro 领域术语；不修改 `CONTEXT.md`。
- 该边界没有需要长期记录的技术锁定或非显然权衡；不新增 ADR。

## Out of Scope

- command framework、handler table、通用 command 对象或额外 action abstraction。
- 测试专用 public/private header、条件编译 API 或直接访问 opaque Application state。
- 改变 settings、manual time、brightness、touch、显示唤醒或持久化的用户可观察行为。
- 为 numeric settings 新增批量调整入口。
- 重构整个 `dispatch` 条件分支或改变 platform/view 分层。
