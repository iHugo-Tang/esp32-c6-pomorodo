# 统一 Codex-only Agent Status 领域与协议边界

Status: complete

## Problem Statement

设备、固件和 host 工具已经通过独立 broker 同时处理 Codex Workspace 状态，但部分领域类型、设置、界面文案、平台接口和测试仍沿用 `OpenCode status` 名称。这个名称把通用 Agent Status 错误地绑定到已不再需要的 OpenCode 来源，并让 `OC1` 线路协议细节泄漏进 application 与 view。

broker 仍保留双 client IPC、OpenCode 来源和旧的 OpenCode 状态目录，尽管产品只需要 Codex Workspace。当前未发布的持久化格式也继续接受前一开发版本，使一次彻底的领域统一被兼容代码拖累。

## Solution

产品只支持 Codex Workspace 作为 Agent Source。设备设置、状态模型、application、view、broker 和当前产品文档统一使用 Agent Status 语言；OpenCode 来源与 broker IPC 的 `client` 维度被删除。

固件把 Agent Status 行为与 `OC1` 线路协议拆成两个清晰 seam：领域模块维护状态、优先级、确认、超时与唤醒；协议 adapter 独占 `OC1` 的解析和格式化。现有线路协议及用户可观察行为保持不变。

host 状态迁移到 `$XDG_STATE_HOME/pomodoro/agent-status`。旧目录由用户在实现前手动删除，运行时代码不负责迁移或删除。当前未发布的 Settings 与 persisted-state 格式升级且不兼容旧开发格式；首次正式发布后的升级仍遵守既有数据保留决策。

## User Stories

1. 作为 Codex 用户，我希望设备把当前功能称为 Agent Status，以便名称准确描述 Codex Workspace 状态。
2. 作为 Codex 用户，我希望 Settings 显示 `Agent` 分区，以便功能不再看起来属于 OpenCode。
3. 作为 Codex 用户，我希望开关显示为 `Agent status`，以便明确它控制设备上的 Agent Status 集成。
4. 作为 Codex 用户，我希望 Agent Status 开关默认开启，以便安装后无需额外配置即可显示状态。
5. 作为 Codex 用户，我希望关闭 Agent Status 后隐藏状态圆，以便可以禁用该功能。
6. 作为 Codex 用户，我希望关闭 Agent Status 后停止处理状态与状态触发的唤醒，以便禁用行为完整一致。
7. 作为 Codex 用户，我希望重新开启 Agent Status 后等待当前 Workspace 重新同步，以便不显示陈旧状态。
8. 作为 Codex 用户，我希望 Main 的状态圆继续显示 Error、Waiting、Working、Completed 或 Idle，以便一眼看到 Selected Agent Source 的聚合状态。
9. 作为 Codex 用户，我希望状态优先级继续为 Error、Waiting、Working、Completed、Idle，以便多个成员同时活动时结果稳定可预测。
10. 作为 Codex 用户，我希望可以确认 Error 或 Completed，以便看到同一 revision 中的下一优先级状态。
11. 作为 Codex 用户，我希望确认只影响当前 revision，以免清除后来发生的新状态。
12. 作为 Codex 用户，我希望 Completed 继续短暂显示后回到 Idle，以便完成反馈可见但不会永久占据状态圆。
13. 作为 Codex 用户，我希望有效的新状态变化继续唤醒显示，以便无需触摸设备即可看到需要关注的 Agent 状态。
14. 作为 Codex 用户，我希望心跳丢失后状态圆继续按既有超时规则隐藏，以免显示已经失联的 Workspace。
15. 作为 Codex 用户，我希望 Agent Status 变化不改变 Pomodoro Mode、Set Progress、Statistics 或当前倒计时。
16. 作为 Codex 用户，我希望不同 Codex Workspace 仍是不同 Agent Source，以便显式选择其中一个显示在设备上。
17. 作为 Codex 用户，我希望 Selected Agent Source 暂时消失时仍保留选择并等待恢复，以免设备自动切换到其他 Workspace。
18. 作为 Codex 用户，我希望 `$pomodoro-select` 继续选择当前 Workspace，以便现有操作方式不变。
19. 作为固件维护者，我希望 Agent Status 领域模块不认识 `OC1` 文本，以便协议变更不会污染状态行为。
20. 作为固件维护者，我希望 `OC1` adapter 集中解析 STATE 并格式化控制与 ACK 行，以便所有线路格式知识具有 locality。
21. 作为固件维护者，我希望 platform interface 明确传输的是 OC1 行，以便调用方不会把传输格式误当领域概念。
22. 作为固件维护者，我希望状态 mask 和五态类型使用 Agent Status 名称，以便代码与 glossary 一致。
23. 作为固件维护者，我希望 Agent Status 与 OC1 分别有自己的行为测试，以便一个模块重构不会破坏另一个模块的测试。
24. 作为 broker 维护者，我希望 Agent Source 只由 Workspace group identity 标识，以便删除恒定且无意义的 client 维度。
25. 作为 broker 维护者，我希望 report、select、remove 和响应 schema 不再包含 client，以便 IPC 与 Codex-only 产品一致。
26. 作为 broker 维护者，我希望持久化 selection 时只保存 Workspace identity 与 revision，以便状态文件没有已删除的 OpenCode 概念。
27. 作为 broker 维护者，我希望未知或旧式 IPC 字段继续被严格拒绝，以便 schema 变化不会被静默兼容。
28. 作为本地工具用户，我希望 broker、reporter、刷机保留和检查工具共享新的 Agent Status 状态目录，以便串口所有权协调仍然可靠。
29. 作为本地工具用户，我希望旧状态目录不会被新代码自动迁移，以便新实现从明确的空状态开始。
30. 作为本地工具用户，我希望新代码不会自动删除用户目录，以便破坏性清理由我显式执行。
31. 作为开发固件用户，我接受旧开发格式中的 Settings、Timer 和 Statistics 被重置，以便实现无需携带未发布格式的迁移逻辑。
32. 作为开发固件用户，我希望旧格式被当作空存储正常启动，而不是进入 Storage Error。
33. 作为未来正式版本用户，我希望首次发布后的正常升级仍保留 Settings、Timer 和 Statistics，以便开发期重置不会削弱正式数据承诺。
34. 作为文档读者，我希望当前 README、产品说明和原型说明统一使用 Agent Status，以便文档与设备一致。
35. 作为架构维护者，我希望历史 ADR 和已取代 issue 保留 OpenCode 原文，以便仍能追溯功能演化。
36. 作为架构维护者，我希望新的 ADR 明确记录 Codex-only 决策，以便未来不会无意恢复 speculative multi-client 支持。

## Implementation Decisions

- Agent Source 只支持 Codex Workspace；OpenCode Process 从当前领域模型、broker schema 和当前产品行为中删除。
- 不保留 OpenCode adapter，也不为未来 host 类型预留 client、factory、registry 或通用 adapter interface。
- 不同 Codex Workspace 继续通过各自的 group identity 形成独立 Agent Source；成员聚合、选择和 revision 规则保持不变。
- broker 的 report 请求包含 Workspace group identity、member identity、PID、started-at 与 mask；select 只包含 Workspace group identity；remove 包含 Workspace group identity与 member identity。
- broker 响应中的 selected 变为 Workspace group identity 或 null，不再返回 client 与 group 的复合对象。
- broker 仍严格验证每种请求的精确字段；携带旧 client 字段的请求按额外字段拒绝。
- broker 持久化仍只保存 selected 与 revision；selected 使用单一 Workspace group identity。
- host 状态目录改为 `$XDG_STATE_HOME/pomodoro/agent-status`，默认 `~/.local/state/pomodoro/agent-status`。broker socket、协调状态和 flash reservation 全部位于该目录。
- 旧的 `opencode/pomodoro-status` 状态目录由用户在实现前停止旧 broker 后手动删除。新代码不探测、不迁移、不删除旧目录。
- 个人插件、reporter、broker、前台入口、刷机 wrapper 和相关检查统一使用新目录。
- 固件建立 Agent Status 领域模块，拥有 enabled、connected、revision、mask、本地确认、Completed 展示期限、心跳期限和 wake-pending 行为。
- Agent Status 领域模块使用 Agent Status 命名的状态、mask bits、常量和 public interface；它只接收已解析的 revision、mask 和 wall time。
- 固件建立 OC1 协议 adapter，独占 `OC1` prefix、最大行长、STATE 解析、ENABLED/DISABLED 格式化与 ACK 格式化。
- OC1 adapter 是具体线路协议模块，不新增可替换 protocol port；当前只有一个真实协议，不为假设中的第二协议增加间接层。
- application 通过 OC1 adapter 将协议输入转换为 Agent Status 更新，并通过 adapter 生成所有协议输出；application 不保存或拼接 `OC1` 字符串。
- platform interface 使用 OC1 传输语义命名的 read-line 与 write-line 操作；USB buffering 与 screenshot multiplexing 继续属于 platform implementation。
- application action、view state、settings 字段和 UI object 名称统一为 Agent Status。
- Settings 使用 `Agent` 分区和 `Agent status` switch；Main 状态圆的尺寸、位置、颜色、触控和 pixel shift 保持不变。
- Error、Waiting、Working、Completed、Idle 优先级，Completed 五秒展示，十秒心跳超时，三秒控制心跳，ACK 规则和显示唤醒行为保持不变。
- `OC1 STATE`、`OC1 ENABLED`、`OC1 DISABLED`、`OC1 ACK` 的线路格式与语义保持不变；不设计新协议或版本。
- Settings version 与顶层 persisted-state version 均升级。旧开发格式不执行迁移，并被视为空存储。
- 前一 Settings 版本的兼容判断和迁移实现删除；Snapshot 与 Statistics 的独立格式未改变，因此不做无意义的版本升级。
- 本次开发期 reset 不 supersede 正式固件更新的数据保留承诺；首次正式发布后的每个受支持升级仍需迁移上一版本。
- 当前 README、产品 prompt、prototype description、硬件/刷机说明和当前 Agent Status spec 使用 Agent Status 语言及新目录。
- 历史 ADR-0010、ADR-0011 与已完成或已取代的 OpenCode `_scratch` 记录保留原文。
- ADR-0015 是 Codex-only Agent Source 与简化 IPC identity 的权威决策，并部分取代 ADR-0012 的双来源表述。

## Testing Decisions

- 测试只验证 public interface 可观察行为，不断言私有 helper、文件内部调用顺序或具体拆分方式。
- Agent Status 领域模块是一个独立 seam；测试初始化、状态应用、优先级、stale revision、本地 ACK、Completed 期限、心跳超时、enable/disable、connected 与 wake-pending 行为。
- OC1 协议 adapter 是一个独立 seam；测试合法 STATE、非 OC1 输入、未知版本、未知命令、缺失/额外 token、数字溢出、非法 mask、最大行长，以及 ENABLED/DISABLED/ACK 的精确格式。
- Agent Status 行为测试不传入协议文本；OC1 测试不触达 Agent Status 内部状态。
- application dispatch 是最高层固件编排 seam；使用现有 fake platform 验证协议输入驱动 view、状态变化唤醒、ACK 输出、Settings 开关、持久化和 Pomodoro 状态隔离。
- application 测试只保留跨模块编排行为，不重复 OC1 parser 的完整输入矩阵或 Agent Status 状态机的全部分支。
- persistence public interface 验证旧 Settings/顶层 state version 被当作空存储，而当前版本能够 pack、journal、load 和 unpack。
- broker core seam 验证 Codex-only request schema、Workspace 隔离、成员聚合、选择、租约、恢复、revision 和持久化 selection。
- 真实 Unix socket seam 验证旧 client 字段被拒绝、新响应 shape、权限、单实例、陈旧 socket 与损坏状态 fail-closed。
- reporter/plugin 集成 seam 验证所有 report、select 和 remove 请求使用新 schema，并继续覆盖 hook 状态映射、subagent 聚合、控制 turn 与按需启动。
- serial publisher seam 继续验证 OC1 ENABLED/DISABLED、心跳、ACK、reservation、重试和断线；线路协议行为不因领域重命名而改变。
- flash reservation 自检使用新状态目录，并继续验证获取、清理、失败和有界超时。
- 现有 native C assert tests、application fake platform、Node assert checks 和 broker fake transport 是新增测试的 prior art。
- 完成后执行全部 native host tests、插件自检、broker 自检、flash reservation 自检和完整 ESP-IDF firmware build。
- 真机验收 Settings 文案、开关、状态圆五态、确认、屏幕唤醒、失联隐藏、Codex Workspace 选择和刷机串口释放。

## Out of Scope

- OpenCode Process、OpenCode adapter 或任何其他 Agent Source 类型。
- 为未来 host 类型保留 client 字段、插件 registry、factory 或通用协议 abstraction。
- 更改 OC1 线路格式、版本、mask 表示、心跳频率或 ACK 语义。
- 新增网络协议、BLE、Wi-Fi、OTA 或远程 Agent Status。
- 改变五态优先级、Completed 展示期限、心跳超时、显示唤醒或 selection 行为。
- 自动迁移旧 host 状态目录。
- 运行时代码自动删除旧 host 状态目录或外部 OpenCode 插件安装。
- 迁移当前未发布的 Settings、Timer、Statistics 或 persisted-state 数据。
- 放弃首次正式发布后的数据保留和版本迁移承诺。
- 重写历史 ADR、已完成 issue 或已取代 spec 中的 OpenCode 术语。
- 改变 Pomodoro、Pomodoro Set、Statistics Day 或任何计时行为。

## Further Notes

- 用户应在实现前停止旧 broker，并手动删除默认旧状态目录 `~/.local/state/opencode/pomodoro-status`；新实现从空的 Agent Status host 状态开始。
- `OC1` 是保留的历史线路协议名，不再表示产品支持 OpenCode Agent Source。
- `CONTEXT.md` 已将 Agent Source 收窄为 Codex Workspace；ADR-0015 记录该不可见于单一代码位置的架构决定。
- 本 spec 取代现有 Agent Status spec 中关于 OpenCode client、双 client identity、旧状态目录及“后续实现 OpenCode adapter”的内容；其余 broker、Codex adapter 与 OC1 行为继续有效。
