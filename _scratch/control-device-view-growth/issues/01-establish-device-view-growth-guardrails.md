# 01 — 建立 Device View 增长护栏

**What to build:** 为未来 Device View 工作建立可执行的维护约束，使新页面先在现有模块内形成清晰的创建与渲染归属，并只在出现真实生命周期、内存 ownership、状态泄漏或持续横切修改时提取页面模块；当前可维护的实现保持不动。

**Blocked by:** None — can start immediately.

**Status:** complete

- [x] 当前 Device View 不因行数、context fields 或 LVGL object 数量而拆文件、重排 context 或新增 page structs。
- [x] 新 Application Page 或全屏 phase 必须从一开始拥有同文件 create/render pair；overlay 继续保持独立 create/render helper。
- [x] 现有页面只在新增或删除控件、callback、页面状态或渲染分支前做同文件 prefactor；文案、颜色和单个布局调整不触发重构。
- [x] 页面 extraction 只由独立生命周期、内存 ownership、重复页面状态泄漏、持续横切修改、merge conflicts 或真实测试 seam 需求触发。
- [x] 未来 extraction 保持 Device View public interface 不变，不公开内部 context 或 LVGL objects。
- [x] 不新增 LOC CI gate、fake LVGL、renderer test framework、page registry、factory、handler table 或其他 speculative abstraction。
- [x] 结构性 Device View 变更要求 native host tests、完整 ESP-IDF build，以及与视觉、触摸、gesture、orientation、display lifecycle 或内存相关的目标设备验收。
- [x] 增长规则记录在本 feature 的 spec 与 ticket 中；`CONTEXT.md`、ADR 和当前生产代码保持不变。

## Comments

- 2026-08-23: Recorded the agreed growth and extraction rules in the feature spec and this ticket. Device View remains at its existing 1,972-line implementation with the same six-operation public interface; no production code, context layout, LVGL ownership, test framework, glossary, or ADR changed. The complete native host suite passes 5/5.
