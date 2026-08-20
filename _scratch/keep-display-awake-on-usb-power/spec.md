# Keep Display Awake on USB Power

Status: ready-for-agent

## Problem Statement

设备在 USB 供电时仍按电池供电策略运行：闲置两分钟后自动调暗，Ready、Paused 和 Completion Transition 状态下闲置五分钟还会关闭显示。

USB 已持续供电时不需要通过降低亮度或自动熄屏节省电量。现有行为会降低设备作为桌面计时器时的可见性，并要求用户反复唤醒显示。

## Solution

当设备处于 `USB Power` 状态时，停止自动调暗和自动熄屏，显示保持用户选择的正常亮度。

插入 USB 后，若显示已因自动省电而调暗或关闭，则在下一次状态更新时恢复正常亮度并唤醒显示。拔出 USB 后，从拔出时刻重新计算两分钟调暗及五分钟熄屏窗口。

用户主动执行的显示熄屏继续有效。AMOLED pixel shift 及其他显示保护行为保持不变。

## User Stories

1. 作为设备用户，我希望 USB 供电时显示保持正常亮度，以便将设备持续用作桌面计时器。
2. 作为设备用户，我希望 USB 供电时 Running 状态不再于两分钟后降至 60% 亮度。
3. 作为设备用户，我希望 USB 供电时 Ready 状态不再自动调暗或熄屏。
4. 作为设备用户，我希望 USB 供电时 Paused 状态不再自动调暗或熄屏。
5. 作为设备用户，我希望 USB 供电时 Completion Transition 不再自动调暗或熄屏。
6. 作为设备用户，我希望该规则在所有页面和首次时间设置阶段一致生效。
7. 作为设备用户，我希望因自动省电而调暗的显示在插入 USB 后恢复正常亮度。
8. 作为设备用户，我希望因自动省电而关闭的显示在插入 USB 后恢复显示。
9. 作为设备用户，我希望用户主动关闭显示的行为保持不变。
10. 作为设备用户，我希望拔出 USB 后拥有完整的两分钟正常亮度时间，而不是立即调暗。
11. 作为设备用户，我希望拔出 USB 后现有五分钟自动熄屏规则恢复生效。
12. 作为设备用户，我希望“正在充电”但没有 `USB Power` 时不被错误视为持续外部供电。
13. 作为设备用户，我希望 USB 连接状态不参与显示省电判断。
14. 作为设备用户，我希望 AMOLED pixel shift 在 USB 供电时继续工作。
15. 作为设备用户，我希望 Pomodoro、Pomodoro Set、计时、统计和持久化规则不受影响。
16. 作为维护者，我希望复用现有设备状态和显示生命周期策略，不引入新的电源状态系统。
17. 作为维护者，我希望 USB 插拔行为由 application orchestrator 的外部可观察测试覆盖。
18. 作为维护者，我希望该功能不增加设置项、依赖或持久化数据迁移。

## Implementation Decisions

- `USB Power` 定义为设备正在通过 USB 接收外部电源，与电池是否正在充电以及 USB 串口是否连接无关。
- 使用现有设备状态中的 `USB Power` 作为唯一判定来源。
- 在领域词汇表中补充 `USB Power` 的产品定义。
- USB 供电期间，自动显示策略始终选择用户当前设置或预览的正常亮度。
- USB 供电规则适用于 Running、Ready、Paused、Completion Transition、全部页面以及首次时间设置阶段。
- USB 供电时不执行 Running 的 60% 自动调暗，也不执行其他状态的 Idle brightness 或五分钟自动熄屏。
- 用户主动关闭显示优先于 USB 供电策略；周期更新不得自动唤醒用户主动关闭的显示。
- 插入 USB 时，因自动省电而调暗或关闭的显示在下一次设备状态更新时恢复正常亮度并唤醒。
- 拔出 USB 时，将闲置计时起点更新为 USB Power 消失的时刻。
- 拔出后不足 120 秒保持正常亮度；达到 120 秒后恢复现有状态相关调暗规则；非 Running 状态达到 300 秒后恢复自动熄屏。
- 插拔 USB 不视为触摸或 UI 操作，也不触发任何 Pomodoro 动作。
- AMOLED pixel shift 与 USB Power 无关并继续运行。
- 不新增设置、持久化字段、数据迁移、依赖或通用电源状态抽象。
- 更新现有产品说明及硬件验收说明中关于两分钟调暗和五分钟熄屏的描述，明确其仅适用于非 USB Power 情况。
- 该决定不新增 ADR。

## Testing Decisions

- 最高自动化测试缝为现有 application orchestrator；它能同时控制时间、设备状态及显示平台回调，并验证用户可观察行为。
- 复用现有显示生命周期测试，不建立新的测试框架或硬件抽象。
- 测试断言显示是否开启、有效亮度和 timer state，不断言私有字段或内部调用顺序。
- 自动化覆盖以下场景：
  - USB Power 下 Ready 状态经过 120 秒和 300 秒仍保持正常亮度且显示开启；
  - USB Power 下 Running 状态经过 120 秒仍保持正常亮度；
  - 仅 `charging` 为真而 `usb_power` 为假时，现有省电规则仍生效；
  - 显示已自动调暗后插入 USB，下一次 tick 恢复正常亮度；
  - 显示已自动熄灭后插入 USB，下一次 tick 唤醒并恢复正常亮度；
  - USB Power 下用户主动熄屏后，后续 tick 不自动唤醒；
  - 拔出 USB 后 119 秒保持正常亮度，120 秒恢复调暗；
  - 非 Running 状态拔出 USB 后 299 秒仍开启，300 秒自动熄屏；
  - USB 插拔不改变 timer state、剩余时间、Pomodoro Set 或统计。
- Running 是独立显示策略分支；Ready 代表共享的非 Running 分支。代码审查须确认 USB Power 判断位于状态分支之前，使 Paused 和 Completion Transition 同样受保护。
- 保留现有用户主动熄屏、触摸唤醒、完成唤醒和 pixel shift 回归测试。
- 运行完整 C 主机测试套件和 ESP-IDF 固件构建。
- 目标设备验证覆盖：
  - Ready、Paused、Running 和 Completion Transition 中插入 USB 后保持正常亮度；
  - 已调暗及已自动熄屏时插入 USB 后恢复显示；
  - USB 供电时用户主动熄屏保持有效；
  - 拔出 USB 后从零重新计算闲置时间；
  - USB 图标、Charging 状态和电池读数保持正确；
  - pixel shift、触摸唤醒及显示生命周期行为不回归。

## Out of Scope

- 修改正常亮度或 Idle brightness 的设置范围、默认值或步进。
- 修改电池供电时的两分钟、五分钟或 Running 60% 阈值。
- 新增“USB 时保持唤醒”设置。
- 根据 Charging、USB 串口、OpenCode Status 或固件更新连接推断 USB Power。
- 禁用用户主动熄屏。
- 禁用 AMOLED pixel shift。
- 修改触摸消费、PWR 长按关机或完成反馈行为。
- 修改 Pomodoro 状态机、统计或持久化格式。
- 修改 PMIC 或板级 USB 检测实现。

## Further Notes

现有设备状态已经提供 `USB Power`，application orchestrator 也已在每次时间读取时刷新该状态。现有显示生命周期策略是最小且正确的修改位置，无需新增服务或抽象。
