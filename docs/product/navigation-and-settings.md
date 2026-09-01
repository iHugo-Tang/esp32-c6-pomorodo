# 导航与设置

## 导航

Main 顶栏左侧是 Menu，中间显示当前 Mode，右侧显示电池状态。

Menu 包含 Statistics、Settings 和 `Clear Set Progress`。从 Menu 打开的页面返回 Menu；从 Menu 返回 Main。确认对话框存在时，其他导航和操作被阻止，直到用户确认或取消。

当前页面关系见 [流程图](../diagrams/pomodoro_prototype_01_flow_navigation.mmd)。

## Settings

Settings 管理计时时长、自动开始、声音、显示、日期时间、Daily Goal、Agent Status、Waiting Sound 和电源操作。

- switch 立即生效并保存；
- 数值先编辑草稿，Confirm 后才保存；
- 亮度立即预览，Confirm 保留，取消恢复原值。

修改 Settings 不应改变正在运行的 Timer；校正日期时间时只重新计算截止时间，剩余时长不变。

## switch 行滚动

在 switch 行、文字或开关上垂直拖动，只滚动 Settings 列表，不切换开关。轻点整行或开关只切换一次。普通操作行保持原来的点击和滚动行为。
