# 控制 Device View 的继续膨胀

Status: complete

## Problem Statement

Device View 当前集中创建和渲染 Main、Menu、Statistics、Settings、Numeric stepper、Time setup、dialogs 与 overlays。实现约 1,972 行，单一 context 持有所有 LVGL 对象，但对外 interface 仍然很窄，页面共享 shell、input dispatch、gesture、pixel shift 和 confirmation 行为也具有真实耦合。

仅按行数拆文件不会减少 eager-created LVGL 对象、SRAM 使用或共享状态，反而会迫使内部 context 和对象引用跨文件暴露。当前代码仍可维护，不值得为了文件长度进行机械拆分；需要控制的是未来页面继续向集中式 create/render 分支无界增长。

## Solution

本轮不修改生产代码。把未来 Device View 工作约束为“先在现有模块内形成页面内聚，再在出现真实 ownership 信号时提取模块”。新页面或新的全屏 phase 从一开始拥有同文件的 create/render pair；现有页面只在结构性修改前做同文件 prefactor。

不设置行数、字段数或 LVGL 对象数阈值，不重排当前 context，不添加形式化 page structs、空 helper、fake LVGL、测试框架或 CI line-count gate。

## Maintenance Stories

1. 作为固件维护者，我希望新页面有明确的创建和渲染归属，以免继续扩大集中式页面分支。
2. 作为固件维护者，我希望小型文案和样式修改保持小型，以免增长规则制造无关重构。
3. 作为固件维护者，我希望只有真实生命周期、内存或交互问题才触发拆文件，以免按行数制造浅模块。
4. 作为设备用户，我希望未来 UI 重构不改变触摸、显示生命周期、pixel shift、orientation 或计时行为。
5. 作为硬件维护者，我希望页面拆分真正改善 LVGL object ownership，而不是只移动代码且保持相同内存风险。

## Growth Rules

- “新页面”包括新的 `Application Page` 和新的全屏 phase。Dialog、warning、volume feedback 等 overlay 不算页面，但必须维持独立的 create/render helper。
- 新页面在 Device View 实现文件内从一开始提供 `create_<page>()` 与 `render_<page>()`；顶层 create 和 render router 只负责调用，不把页面主体继续加入集中式辅助页分支。
- 现有 Main、Statistics、Settings、Numeric 与 Time setup 保持原状。新增或删除控件、callback、页面状态或渲染分支前，先把被修改页面整理为同文件 create/render pair。
- 修改文案、颜色或单个布局值不触发页面 prefactor。
- 当前 flat context 保持不变；不为字段分组创建没有独立 ownership 的嵌套 page structs。
- Future page extraction 必须保持 Device View public interface 不变，不公开全局 context 或 LVGL objects。跨文件协作只使用最小内部参数或 private header。

## Extraction Triggers

出现以下任一信号时，评估提取对应页面；若涉及独立对象生命周期或内存 ownership，则必须解决 ownership，而不能只移动函数：

- 页面需要自己的 timer、animation、lazy create/destroy 或其他独立生命周期。
- 辅助页切换再次出现控件 hide/reset、scroll、selection 或样式状态泄漏。
- Page-specific callback 开始根据其他页面状态分支。
- LVGL object 创建出现内存失败或 allocation lockup，需要按页管理对象生命周期。
- 同一页面的连续功能反复要求同时修改 context、create、render 与 gesture、pixel shift 等至少三个横切区域。
- 并行 UI 工作持续在 context、集中式 create 或辅助页 renderer 产生 merge conflict。
- 页面渲染需要独立 host-test seam，且现有 Application view snapshot seam 无法证明行为。

这些信号是设计证据，不是自动阈值；一次小型横切修改本身不要求拆分。

## Verification Decisions

- 不为当前 Device View 引入 fake LVGL 或新的 renderer test framework。
- 未来页面行为继续优先通过 Application interface tests 验证 view snapshot 与 action flow。
- 每次 Device View 结构性修改运行全部 native host tests 与完整 ESP-IDF build。
- 视觉、触摸、gesture、orientation、animation、显示生命周期或内存 ownership 变化必须在目标设备验收。
- Review 检查新页面是否具有 create/render pair、page-specific callback 是否引入跨页分支，以及拆分是否扩大 public interface。

## Documentation Decisions

- 本规格与后续 ticket 是增长规则的记录位置。
- Device View 是实现模块，不是 Pomodoro 领域概念；不修改 `CONTEXT.md`。
- 规则可逆，且 ADR-0004 已记录 UI、领域状态与持久化分层；不新增 ADR。

## Out of Scope

- 现在拆分 Device View 文件或移动现有页面代码。
- 按 LOC、context fields、LVGL objects 或函数数量设置 CI gate。
- 现在重排 flat context 或把字段包装为 page structs。
- 改为 lazy page allocation、动态页面销毁或新的内存管理策略。
- 扩大 Device View public interface、暴露 LVGL objects 或公开内部 context。
- 新增 UI framework、page registry、factory、virtual dispatch 或 handler table。
- 改变任何当前页面、overlay、touch、gesture、pixel shift、orientation 或 timer 行为。
