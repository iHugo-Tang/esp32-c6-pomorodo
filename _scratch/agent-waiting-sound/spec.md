# Agent Waiting Sound

Status: ready-for-human

## Problem Statement

Agent Status 已能用黄色圆点表示 Selected Agent Source 正在 Waiting，但用户没有持续注视 Main 时无法及时察觉 Codex 正在等待问题回答或权限决定。现有声音只表达 Focus 与 Break 完成，也没有独立控制 Agent 通知的设置。

## Solution

当 Agent Status 的有效聚合状态从非 Waiting 进入 Waiting 时，播放一次独立的短单音。首次同步、重新启用或断线重连后以 Waiting 恢复有效，也视为一次新的提示机会。

Settings 的 `Agent` 分区在 `Agent status` 后增加默认开启并持久化的 `Waiting sound` 开关。Waiting Sound 服从全局 Sound 和当前音量，在任何页面及运行中的 Focus 或 Break 期间均可提示，但不改变计时、导航、统计或视觉状态。

## User Stories

1. 作为 Codex 用户，我希望设备在 Agent Status 进入 Waiting 时发出一次声音，以便无需持续注视黄色状态圆。
2. 作为用户，我希望首次同步到 Waiting 时收到提示，以便设备启动后不会漏掉已有的待处理请求。
3. 作为用户，我希望 Agent Status 重新开启或断线重连后仍为 Waiting 时收到提示，以便恢复连接后重新注意它。
4. 作为用户，我希望确认 Error 后显露 Waiting 时收到提示，以便知道红色状态之后还有待处理事项。
5. 作为用户，我希望重复心跳及持续黄色状态不会反复发声。
6. 作为用户，我接受设备只按 Workspace 聚合状态提示，不要求每个成员或每次权限请求分别发声。
7. 作为用户，我希望每次真实的 Working 到 Waiting 转换都能提示，不被冷却时间吞掉。
8. 作为用户，我希望 Waiting Sound 在 Main、Settings、Statistics、数值编辑及确认对话框期间都能工作。
9. 作为用户，我希望 Waiting Sound 在 Focus 或 Break 运行期间不会暂停或改变计时。
10. 作为用户，我希望能独立关闭 Waiting Sound，而不关闭 Agent Status 显示。
11. 作为用户，我希望 Waiting Sound 默认开启并在重启后保留选择。
12. 作为用户，我希望全局静音或零音量同时静音 Waiting Sound。
13. 作为用户，我希望在已经 Waiting 时开启提示、解除静音或调高音量不会补播旧提示。
14. 作为用户，我希望 Waiting Sound 与 Pomodoro 或 Break 完成音冲突时，计时完成音优先且不会丢失。
15. 作为用户，我希望设备音频正忙时可以跳过 Waiting Sound，而不是稍后播放一个失去上下文的提示。
16. 作为用户，我希望 Waiting Sound 失败不会让整个应用进入 Audio Error。
17. 作为用户，我希望黄色状态圆及现有唤醒行为保持不变，不新增浮层、文字或动画。
18. 作为开发期固件用户，我接受此次持久化格式升级重置 Settings、当前计时、Set Progress 和 Statistics。

## Behavior Matrix

| 场景 | 是否播放 |
| --- | --- |
| 有效聚合状态从 Idle、Working、Completed 或 Error 进入 Waiting | 是 |
| 首次有效同步即为 Waiting | 是 |
| Agent Status 重新启用后的首次有效同步为 Waiting | 是 |
| 心跳超时后重连并恢复为 Waiting | 是 |
| 确认 Error 后同一聚合状态显露 Waiting | 是 |
| 相同 revision 与相同 mask 的周期心跳 | 否 |
| 新 revision 但有效状态始终保持 Waiting | 否 |
| 已经黄色时另一个成员也进入 Waiting | 否；现有聚合协议无法区分且不扩展协议 |
| 已经 Waiting 时开启 `Waiting sound` | 否 |
| 已经 Waiting 时解除静音或调高音量 | 否 |
| `Waiting sound` 关闭、Agent Status 关闭或全局音量为零 | 否 |
| 音频设备正忙 | 否；不排队补播 |

## Implementation Decisions

- 使用 `Waiting Sound` 作为领域术语，UI 文案为 `Waiting sound`。
- `Waiting sound` 位于 Settings 的 `Agent` 分区，紧跟 `Agent status`。
- 开关默认开启并独立持久化；`Agent status` 关闭时仍显示且可编辑，但不会产生提示。
- Waiting Sound 是全局通知，不依赖当前页面、黄色圆点当前是否实际绘制或 Timer Mode。
- 触发边界以应用可观察的有效聚合状态变化为准；比较转换前后的有效性与 resolved Agent Status，而不是原始 `OC1` 文本、单个成员事件或心跳次数。
- 首次同步、重新启用和断线重连从无有效状态恢复到 Waiting，等价于进入 Waiting。
- Error ACK 后 resolved 状态从 Error 变为 Waiting 时触发，即使没有新的 host revision。
- 同色新 revision、重复选择导致的黄色 epoch 更新以及持续黄色期间的成员变化不触发。
- 设置或音量的变化只影响未来触发，不检查或补播当前 Waiting。
- Waiting 使用用户提供的约 0.50 秒 `warning` 音效；Focus 与 Break 完成使用约 1.85 秒的 `rest` 音效。两个 MP3 在构建前转换为设备原生的 16 kHz 单声道 PCM 并内置，播放时扩为双声道，不引入运行时 MP3 解码依赖。
- 使用播放时的当前音量；`volume == 0` 时静默成功。
- Waiting Sound 是 best-effort：音频忙或播放失败时丢弃，不排队、不重试，也不把 application 置为 Audio Error。
- 音频仲裁保证 Focus 与 Break 完成音优先且不会因 Waiting Sound 丢失；Waiting Sound 不得占用或覆盖待播放的完成反馈。
- 除内置 `warning` 与 `rest` 声音资源外，不新增外部依赖、浮层、文字、动画或新的 Main 控件。
- 不修改 Agent Status 优先级、颜色、确认、Completed 到期、心跳超时、显示唤醒或 Selected Agent Source 行为。
- 不修改 Codex plugin、broker、成员聚合、revision 生成或 `OC1` 协议。现有协议只能支持聚合级提示。
- Settings 与顶层 persisted-state 格式升级；沿用开发期的旧格式视为空策略，不实现迁移。升级后 Settings、Timer snapshot、Set Progress 与 Statistics 全部恢复默认空状态，新 `Waiting sound` 默认为开启。
- 更新产品说明与硬件验收说明，记录 Waiting Sound、设置、提示音和重置行为。
- 该功能不满足 ADR 的不可逆性、意外性与真实架构取舍三项条件，因此不新增 ADR。

## Testing Decisions

- Agent Status 领域测试继续只处理状态、优先级、确认、超时与有效性，不引入音频 platform 或 `OC1` 文本。
- 最高自动化测试 seam 为 application dispatch；fake platform 增加可观察的 Waiting Sound 请求与音频忙/失败行为。
- Application 测试覆盖：
  - Idle、Working、Completed 与 Error 到 Waiting 各播放一次；
  - 首次同步、重新启用与超时重连到 Waiting 各播放一次；
  - Error ACK 显露 Waiting 播放一次；
  - 相同心跳、同色新 revision、持续黄色成员变化不重复播放；
  - Settings、Statistics、数值编辑和确认对话框期间仍播放且导航不变；
  - Running Focus 与 Break 期间播放且 Timer、Set Progress 和 Statistics 不变；
  - Waiting Sound 开关、Agent Status 开关、全局静音及当前音量的组合；
  - 已 Waiting 时开启开关、解除静音或调高音量不补播；
  - 音频忙或 Waiting sound 失败被丢弃且 application 不进入 Audio Error；
  - 与 Focus/Break 完成同时发生时完成音保留且 Waiting Sound 可丢弃；
  - 开关默认开启、切换后保存并在当前格式重启后恢复；
  - 上一 Settings/顶层 state 格式被视为空，并重置 Settings、Timer、Set Progress 与 Statistics。
- 测试外部可观察行为，不断言私有 helper、内部调用顺序、LVGL object 结构或具体音频任务实现。
- Platform/board seam 覆盖声音资源映射、零音量、busy 处理及完成音优先仲裁。
- 运行完整 native host test suite 与 ESP-IDF firmware build。
- 目标设备验证开关文案和默认值、音色与音量、各页面、计时期间、重连、静音、声音冲突、重启持久化以及四种 UI Orientation。

## Out of Scope

- 为每个 Codex 成员、问题或权限请求分别提示。
- 修改 plugin hooks、broker 聚合或 `OC1` 线路格式。
- 为 Waiting Sound 增加音量、音色、频率、时长或冷却时间设置。
- 提示 Working、Completed、Idle 或 Error。
- 改变现有 Focus、Break 完成音或 Agent Status 显示颜色。
- 增加通知历史、文字详情、浮层、动画、手机通知、Wi-Fi、BLE 或网络服务。
- 为旧开发持久化格式保留或迁移任何 Settings、Timer、Set Progress 或 Statistics 数据。
- 重构为通用通知、事件总线、音效注册表或可替换音频框架。

## Further Notes

当前设备接收的 `OC1 STATE` 只包含 Selected Agent Source 的聚合 mask。同一黄色期间新增 Waiting 成员不会改变 resolved 状态，因而不会产生第二次提示；这正是本功能选择的聚合级语义。
