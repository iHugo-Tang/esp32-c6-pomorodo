# OpenCode Agent Status Handoff

Status: confirmed

## Goal

让 Pomodoro 设备显示 OpenCode 的 Agent Status，并由用户在 OpenCode 中输入 `/pomodoro-select` 激活当前 OpenCode Workspace 的监听。

确认依据：用户在 2026-09-06 明确提出上述目标，并在首轮回答中把来源边界确认为当前 OpenCode Workspace。

## Scope

- OpenCode 插件接收 OpenCode lifecycle events，并把所选来源的五态汇总结果交给现有 Agent Status 链路。
- `/pomodoro-select` 选择或激活当前 OpenCode 来源。
- OpenCode 作为第二种受支持来源与现有 Codex 支持共存。
- OpenCode Workspace 是选择和状态聚合边界，并与相同路径的 Codex Workspace 使用不同 identity。
- 插件和 `/pomodoro-select` 采用用户级安装，使任意项目可用。
- 选择保持并等待同一 OpenCode Workspace 恢复；首次激活必须来自直接 `/pomodoro-select`，重启后由恢复上报的成员重新建立状态。
- 保持需求、领域词汇、产品文档和架构决策一致。

## Exclusions

以下边界继承自当前 Agent Status 设计；除非访谈明确改变，否则不属于本次目标：

- 不通过 Agent Status 通道传输 prompt、代码、回复或错误详情。
- 不由 Agent Status 改变 Mode、Pomodoro、Set Progress、Statistics 或倒计时。
- 不在设备上浏览或选择宿主来源。
- 不让宿主适配器直接占用 USB 串口；现有 broker 仍是唯一串口所有者。
- 不修改 `OC1` 线路协议或设备端五态显示语义。

## Requirements

- **OCAS-R001 OpenCode status publishing**: OpenCode Workspace 可把 Idle、Working、Waiting、Error 和 Completed 状态发布到设备现有 Agent Status 显示。
- **OCAS-R002 Explicit activation**: 用户输入 `/pomodoro-select` 后，当前 OpenCode Workspace 成为 Selected Agent Source；选择保持到另一次显式选择，来源暂时消失时等待同一 Workspace 恢复。
- **OCAS-R003 Isolation**: OpenCode 状态不得改变任何 Pomodoro 计时、进度、统计或设置数据。
- **OCAS-R004 Broker boundary**: OpenCode 适配器通过本地 IPC 上报；独立 broker 继续独占 USB、分配 revision、维持租约并协调烧录。
- **OCAS-R005 Failure containment**: 插件、broker、状态目录或串口不可用时不得阻塞或中止 OpenCode 工作，也不得产生第二个串口所有者。
- **OCAS-R006 Explicit source stability**: 在用户选择另一来源前，Session 关闭、OpenCode 退出、插件重启或租约超时不得让无关来源静默接管；同一 OpenCode Workspace 恢复上报时继续显示。
- **OCAS-R007 Coexistence**: OpenCode 和 Codex 都是受支持的来源；新增 OpenCode 不得移除或破坏现有 Codex 选择和状态上报。
- **OCAS-R008 User-level availability**: OpenCode 插件与 `/pomodoro-select` 以用户级能力安装，可在任意项目中使用；Pomodoro 仓库保存权威源码和安装说明但不重复自动加载项目副本。
- **OCAS-R009 Command purity**: `/pomodoro-select` 直接执行插件选择逻辑，不创建 Task、prompt、Session message 或模型请求，也不产生伪 Working/Completed 周期。
- **OCAS-R010 Explicit first activation**: OpenCode Workspace 在首次执行 `/pomodoro-select` 前不得因插件加载或普通 lifecycle report 自动成为 Selected Agent Source。
- **OCAS-R011 Workspace aggregation**: 所选 OpenCode Workspace 聚合其全部存活 Session 与 subagent，包括分布在多个 OpenCode 进程中的成员；所有成员消失时保留 Workspace 选择并等待恢复。
- **OCAS-R012 OpenCode lifecycle mapping**: busy/retry 映射 Working，权限或 Question 等待映射 Waiting，Session 错误或结构化工具失败映射锁存 Error，成功 idle 映射 Completed，新建、已确认完成或无活动映射 Idle；新真实工作清除对应成员 Error，Workspace 继续使用既有五态优先级。
- **OCAS-R013 Selection feedback**: 直接选择命令显示成功、等待接管或失败反馈；启动或选择失败不得中断 OpenCode，设备保持原 Selected Agent Source，反馈不得泄露工作内容。
- **OCAS-R014 TUI selection scope**: 首版只承诺在 OpenCode TUI 中提供直接 `/pomodoro-select`；Desktop、Web 和非交互 CLI 不提供等价选择入口。

## Design Baseline

- 固件和 `OC1` 已与具体宿主解耦，可直接复用五态、ACK、五秒 Completed 到期和十秒心跳超时。
- broker 已提供严格的 `report`、`select`、`remove` JSON-line IPC，`groupId` 与 `memberId` 在代码中是 opaque string。
- 现有 Codex 插件按 Codex Workspace 聚合成员；`$pomodoro-select` 是当前 Codex 选择入口。
- OpenCode 1.18.29 的当前公开插件 API 提供 `session.status`、`session.idle`、`session.error`、权限、消息、工具与 command events；当前公开自定义 `/name` 命令是 prompt template，而非已证明可直接运行插件代码的零模型命令。
- OpenCode 1.18.29 安装的 `@opencode-ai/plugin/tui` 另有 TUI plugin API；其 legacy `api.command.register` 支持 `slash.name` 与直接 `onSelect`，可以在不提交 prompt、不创建消息且不请求模型的情况下执行选择。该入口已标记 deprecated，当前可用但需要版本回归验收。

## Decision Rationale

- 复用 broker 而不是恢复历史 OpenCode 进程串口竞选，可保留单一串口所有权、烧录安全与来源选择语义，同时把新增范围限制为宿主适配器和来源模型。
- 由于 [ADR-0015](../adr/0015-agent-status-only-supports-codex-workspaces.md) 明确排除 OpenCode，本功能若获确认必须通过新的 ADR 取代其 Codex-only 来源边界，不能静默修改历史决定。
- 历史 OpenCode 实现可用于恢复事件映射和验收场景，但其自动回退、直接串口所有权和旧状态目录已被后续 broker 设计取代，不应默认恢复。
- 同路径跨宿主聚合会让未选择的 Codex 活动改变 OpenCode 显示，因此两种 Workspace 保持不同 identity，并由既有 Selected Agent Source 规则确保同时只有一个来源有效。
- 标准 Markdown slash command 会提交 prompt，违反零模型要求；1.18.29 的 TUI plugin direct action 可以满足要求，但其 deprecated 状态需要明确的兼容性失败检查。

## Design

- OpenCode server plugin 按规范化 Git 根目录或非 Git 工作目录生成 OpenCode Workspace identity，并以 Session 和 subagent 为成员向现有 broker 上报。
- OpenCode 与 Codex identity 带宿主类型边界；相同路径不会合并。broker 仍只把 Selected Agent Source 的 mask 发布给设备。
- OpenCode 普通 lifecycle event 在首次 `/pomodoro-select` 前不能自动选择来源。直接命令选择当前 Workspace、按需启动 broker 并开启新 revision；重复选择也开启新 revision。
- 所选 Workspace 内全部存活成员跨 OpenCode 进程聚合。成员按租约或删除独立移除；全部消失时保留选择，同一 Workspace 再次上报时恢复。
- 用户级 server plugin 负责 lifecycle adapter；用户级 TUI plugin 通过 1.18.29 的 direct slash action 注册 `/pomodoro-select` 并显示 Toast，不存在同名 Markdown prompt command。
- broker、IPC、状态目录、USB 与 flash reservation 沿用当前实现边界；插件异常 fail open for OpenCode、fail closed for device ownership。

## Recommendations

- server plugin、TUI plugin 与 broker client 共享一个可独立检查的 OpenCode adapter core，避免两个插件表面对 Workspace identity 和选择结果作不同解释。
- 安装检查应验证 TUI plugin 已激活、直接 slash API 可用且不存在同名 Markdown command。

## Confirmed Decisions

- **CD-001 Product goal**: 支持 OpenCode 状态，并以 `/pomodoro-select` 激活监听。
  - Evidence: 用户 2026-09-06 的原始请求。
- **CD-002 Existing device safety boundary**: `OC1` 只传状态，Agent Status 不控制 Pomodoro 产品状态。
  - Evidence: 已接受的 ADR-0010 保留边界、当前产品文档与固件实现；本次请求未提出改变。
- **CD-003 Existing serial ownership boundary**: 独立 broker 是唯一 USB 串口所有者。
  - Evidence: 已接受的 ADR-0012 与 ADR-0014；本次请求未提出改变。
- **CD-004 Host coexistence**: OpenCode 作为新增来源与 Codex 支持共存，不替换 Codex。
  - Evidence: 用户 2026-09-06 对 Q1 回答 `yes`，接受“保留 Codex 并新增 OpenCode”的推荐答案。
- **CD-005 OpenCode source boundary**: Agent Source 是当前 OpenCode Workspace，不是单个 Session 或 OpenCode Process。
  - Evidence: 用户 2026-09-06 对 Q2 回答“当前 OpenCode Workspace”。
- **CD-006 User-level installation**: 插件和命令采用用户级安装，仓库保存权威源码。
  - Evidence: 用户 2026-09-06 对 Q3 回答 `yes`，接受用户级安装推荐答案。
- **CD-007 Recovery without fallback**: 所选 OpenCode Workspace 消失时保留选择并等待同一 Workspace 恢复。
  - Evidence: 用户 2026-09-06 对 Q5 回答“等待同一 workspace Session 恢”；结合已确认的 Workspace 来源边界，按等待同一 Workspace 恢复记录，不推导单 Session identity。
- **CD-008 Supported baseline**: 首版以 OpenCode 1.18.29 为最低已验证版本，不承诺未测试旧版本。
  - Evidence: 用户 2026-09-06 对 Q6 回答 `yes`，接受推荐答案。
- **CD-009 Direct command**: `/pomodoro-select` 必须零模型请求、零 Session message，直接执行选择。
  - Evidence: 用户 2026-09-06 对澄清后的 Q4 选择 A。
- **CD-010 Explicit first activation**: OpenCode Workspace 必须先执行 `/pomodoro-select`，不能按首个普通 report 自动选中。
  - Evidence: 用户 2026-09-06 对 Q8 回答 `yes`，接受推荐答案。
- **CD-011 Workspace membership**: 聚合所选 Workspace 内所有存活 Session 和 subagent，包括多个 OpenCode 进程中的成员。
  - Evidence: 用户 2026-09-06 对 Q9 回答 `yes`，接受推荐答案。
- **CD-012 Lifecycle mapping**: 采用 `OCAS-R012` 的五态事件映射、Error 锁存和既有 Workspace 优先级。
  - Evidence: 用户 2026-09-06 对 Q10 回答 `yes`，接受完整推荐映射。
- **CD-013 Selection feedback**: 命令提供成功、等待接管和失败反馈；失败不阻塞 OpenCode、不切换设备来源且不泄露工作内容。
  - Evidence: 用户 2026-09-06 对 Q11 回答 `yes`，接受推荐答案。
- **CD-014 Cross-host isolation**: Codex 与 OpenCode 可同时存在，但选择 OpenCode 后 Codex 状态不再被监听或影响设备；相同路径的两种 Workspace 不聚合。
  - Evidence: 用户 2026-09-06 对 Q7 澄清“选择 opencode 之后，codex 状态就不监听了”。
- **CD-015 TUI-only selection**: 首版 `/pomodoro-select` 只要求 OpenCode TUI。
  - Evidence: 用户 2026-09-06 对 Q12 回答 `tui`。
- **CD-016 No firmware protocol change**: 现有设备 UI、固件状态机与 `OC1` 协议直接复用，本功能不要求固件功能或线路协议变更。
  - Evidence: 用户 2026-09-06 明确确认包含该设计基线和排除项的完整 handoff。
- **CD-017 Final design confirmation**: 本文的目标、范围、排除项、需求、设计、验收标准、失败场景和候选切片获得整体确认。
  - Evidence: 用户 2026-09-06 在收到完整共识与文档链接后明确回答 `yes`。

## Assumptions

None.

## Acceptance Criteria

- **OCAS-AC001** (`OCAS-R001`, `OCAS-R002`): 在一个受支持的 OpenCode Workspace 中执行 `/pomodoro-select` 后，设备显示该 Workspace 随后产生的五态变化。
- **OCAS-AC002** (`OCAS-R001`): Working、Waiting、Error、Completed、Idle 的事件映射和优先级有可运行的 host-side 自动检查。
- **OCAS-AC003** (`OCAS-R003`): OpenCode 状态变化及选择操作不改变运行中的 Pomodoro、Mode、Set Progress、Statistics 或倒计时。
- **OCAS-AC004** (`OCAS-R004`, `OCAS-R005`): OpenCode 与 Codex 同时运行时仍只有 broker 访问串口，烧录保留可使 broker 释放并恢复串口。
- **OCAS-AC005** (`OCAS-R005`): broker 缺失、IPC 响应损坏、状态目录不可用、设备缺失或串口占用时，OpenCode 继续工作且插件静默重试或给出非阻塞反馈。
- **OCAS-AC006** (`OCAS-R006`): 选中来源断开、OpenCode 重启及另一个来源继续上报时，行为符合最终确认的选择生命周期，且不会发生未授权接管。
- **OCAS-AC007** (`OCAS-R002`): `/pomodoro-select` 的可见反馈、是否创建消息/模型请求及重复选择行为符合最终确认的命令契约。
- **OCAS-AC008** (`OCAS-R007`): OpenCode 与 Codex 来源可以分别上报、被显式选择和恢复；新增 OpenCode 后现有 `$pomodoro-select` Codex 流程继续通过自动检查。
- **OCAS-AC009** (`OCAS-R008`): 完全退出并重启 OpenCode 后，至少两个不同项目都能发现并执行同一个用户级 `/pomodoro-select`，且 Pomodoro 项目不会加载重复插件。
- **OCAS-AC010** (`OCAS-R009`): 自动或可审计的 TUI 验收证明 `/pomodoro-select` 直接调用插件选择逻辑，未创建 Task、prompt、Session message 或模型请求。
- **OCAS-AC011** (`OCAS-R010`): 新安装或清空选择后，仅启动 OpenCode 和产生普通工作不会选择 OpenCode Workspace；首次 `/pomodoro-select` 才选择它。
- **OCAS-AC012** (`OCAS-R011`): 同一 Workspace 的两个 Session 位于不同 OpenCode 进程时按优先级聚合；成员分别关闭时只移除自身，全部消失后保留选择，任一同 Workspace 成员恢复后恢复显示。
- **OCAS-AC013** (`OCAS-R012`): host checks 覆盖 busy/retry、权限、Question、Session error、tool error、idle、新工作清除 Error、成员删除和跨成员优先级。
- **OCAS-AC014** (`OCAS-R013`): 选择成功、等待 broker 接管和失败分别产生不含工作内容的可见反馈；失败场景保持原设备来源且 OpenCode 可继续使用。
- **OCAS-AC015** (`OCAS-R008`, `OCAS-R009`): OpenCode 升级验证会检查 TUI slash registration 仍可用；若 1.18.29 的 deprecated legacy 入口被移除，安装检查明确失败而不是退化成 prompt command。
- **OCAS-AC016** (`OCAS-R007`): 同一代码库中 Codex 为 Waiting、OpenCode 为 Working 时，选择 OpenCode 只显示 Working；随后选择 Codex 只显示 Waiting，未选宿主的状态不参与 mask。
- **OCAS-AC017** (`OCAS-R014`): OpenCode TUI 的 slash autocomplete 可发现并直接执行命令；Desktop、Web 和非交互 CLI 不作为首版验收入口。

## Important Failure Scenarios

- **OCAS-F001** (`OCAS-R005`, `OCAS-AC005`): broker 无法启动或连接时，选择失败不得使 OpenCode 命令失败、会话中断或插件崩溃。
- **OCAS-F002** (`OCAS-R004`, `OCAS-AC004`): 两个宿主同时上报或同时选择时，broker 串行化选择并保持单一 serial owner。
- **OCAS-F003** (`OCAS-R006`, `OCAS-AC006`): 所选 OpenCode 工作上下文租约过期时，不得自动显示另一个未选择来源的状态。
- **OCAS-F004** (`OCAS-R001`, `OCAS-AC002`): 延迟、重复或乱序 lifecycle event 不得让旧 revision 覆盖较新的设备状态。
- **OCAS-F005** (`OCAS-R002`, `OCAS-AC007`): 选择命令自身不得被误报为普通 OpenCode 工作的 Working/Completed 周期。
- **OCAS-F006** (`OCAS-R009`, `OCAS-AC010`, `OCAS-AC015`): TUI command API 不可用时不得注册会触发模型的同名 Markdown command 作为静默降级。
- **OCAS-F007** (`OCAS-R007`, `OCAS-AC016`): 未选中的 Codex 或 OpenCode 来源即使继续上报 Error 或 Waiting，也不得改变设备当前 revision 或 mask。

## References

- Domain glossary: [CONTEXT.md](../../CONTEXT.md)
- Current Agent Status requirements: [_scratch/specs/agent-status.md](../../_scratch/specs/agent-status.md)
- Product behavior: [docs/product/agent-status.md](../product/agent-status.md)
- USB boundary history: [ADR-0010](../adr/0010-use-usb-for-opencode-status.md)
- Independent broker: [ADR-0012](../adr/0012-use-independent-agent-status-broker.md)
- Current Codex adapter: [ADR-0013](../adr/0013-use-codex-workspace-lifecycle-adapter.md)
- On-demand broker: [ADR-0014](../adr/0014-start-agent-status-broker-on-selection.md)
- Superseded Codex-only boundary: [ADR-0015](../adr/0015-agent-status-only-supports-codex-workspaces.md)
- Accepted OpenCode source boundary: [ADR-0016](../adr/0016-support-opencode-workspaces-as-agent-sources.md)
- Broker implementation: `plugins/pomodoro-agent-status/scripts/broker.mjs`
- Codex adapter implementation: `plugins/pomodoro-agent-status/scripts/reporter.mjs`

## Candidate Implementation Slices

- **S1 Resolve source model and supersede Codex-only decision**
  - Dependencies: none; this handoff and ADR-0016 are confirmed.
  - Requirements: `OCAS-R002`, `OCAS-R004`, `OCAS-R006`, `OCAS-R007`, `OCAS-R010`.
  - Acceptance: `OCAS-AC006`, `OCAS-AC008`, `OCAS-AC011`, `OCAS-AC016`.
- **S2 Add OpenCode lifecycle adapter and host checks**
  - Dependencies: S1.
  - Requirements: `OCAS-R001`, `OCAS-R004`, `OCAS-R005`, `OCAS-R008`, `OCAS-R011`, `OCAS-R012`.
  - Acceptance: `OCAS-AC001`, `OCAS-AC002`, `OCAS-AC005`, `OCAS-AC009`, `OCAS-AC012`, `OCAS-AC013`.
- **S3 Add `/pomodoro-select` activation path**
  - Dependencies: S1 and shared adapter core from S2.
  - Requirements: `OCAS-R002`, `OCAS-R005`, `OCAS-R006`, `OCAS-R008`, `OCAS-R009`, `OCAS-R010`, `OCAS-R013`, `OCAS-R014`.
  - Acceptance: `OCAS-AC001`, `OCAS-AC006`, `OCAS-AC007`, `OCAS-AC009`, `OCAS-AC010`, `OCAS-AC011`, `OCAS-AC014`, `OCAS-AC015`, `OCAS-AC017`.
- **S4 Integrate shared broker and cross-host selection tests**
  - Dependencies: S2 and S3.
  - Requirements: `OCAS-R004`, `OCAS-R005`, `OCAS-R006`, `OCAS-R007`, `OCAS-R011`.
  - Acceptance: `OCAS-AC004`, `OCAS-AC005`, `OCAS-AC006`, `OCAS-AC008`, `OCAS-AC012`, `OCAS-AC016`.
- **S5 Align product docs and perform hardware acceptance**
  - Dependencies: S4.
  - Requirements: all.
  - Acceptance: all; automated checks do not replace physical-device validation.

These slices are proposals, not formal tickets.

## Open Questions

None in the confirmed current scope.

## Deferred Items

- Direct selection from OpenCode Desktop, Web, or non-interactive CLI. Blocking impact: none for the confirmed TUI-only first release; each frontend requires an equivalent zero-model command contract before inclusion.
- npm publication and remote distribution. Blocking impact: none for user-level local installation from the repository's authoritative source.
