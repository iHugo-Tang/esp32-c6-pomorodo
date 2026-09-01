# ESP32-C6 硬件连接测试

## 测试结果

2026-08-15 在 macOS 上通过串口 `/dev/cu.usbmodem11101` 完成设备通信测试。
测试成功，设备可正常连接，并被识别为 ESP32-C6。

| 项目 | 检测结果 |
| --- | --- |
| 芯片 | ESP32-C6 (QFN40) |
| 芯片修订版本 | v0.2 |
| 功能 | Wi-Fi 6、Bluetooth 5 LE、IEEE 802.15.4 |
| CPU | 单核主 CPU + LP Core，最高 160 MHz |
| 晶振 | 40 MHz |
| USB 模式 | USB-Serial/JTAG |
| Base MAC | `20:6e:f1:16:de:bc` |
| 测试使用的 ESP-IDF | v5.5.5-316-g1a1a5aa6513 |
| 测试使用的 esptool | v4.12.0 |

`esptool` 已成功完成串口连接、芯片类型检测、stub 上传和硬件复位。
最初的连通性测试只读取芯片信息，没有擦除或写入 Flash。

## 固件实机验证

同日使用 `idf.py -p /dev/cu.usbmodem11101 flash` 写入 Main timer 固件；未执行
`erase-flash`，因此 Pomodoro NVS 分区中的 Settings、计时状态和 Set Progress 保留。
启动日志确认：

- SH8601 显示驱动 1.0.2 初始化成功；
- LVGL 使用 9,600 像素（19,200 字节）的单个局部绘制缓冲区；
- CST9217 触摸芯片识别为 `0x9220`，分辨率为 480×480；
- RTC 和 NVS 保存状态恢复成功；
- 恢复后的 Running 计时按绝对时间每秒递减，连续日志从 618 秒递减到 614 秒。

固件构建使用 ESP-IDF v5.5.5，位于项目要求的 `>=5.5.2,<5.6` 范围内。

## 复现测试

先按照
[ESP-IDF v5.5 官方安装指南](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.5/esp32/get-started/linux-macos-setup.html)
激活 ESP-IDF 环境：

```sh
. "$HOME/esp/esp-idf/export.sh"
```

确认串口存在：

```sh
ls -l /dev/cu.usbmodem11101
```

读取芯片信息：

```sh
python -m esptool --port /dev/cu.usbmodem11101 chip_id
```

预期输出应包含：

```text
Detecting chip type... ESP32-C6
Chip is ESP32-C6 (QFN40) (revision v0.2)
USB mode: USB-Serial/JTAG
```

ESP32-C6 没有传统 Chip ID，因此 `esptool` 会读取 MAC 地址作为设备标识；相关警告是正常现象。

## 工程目标

为该设备配置 ESP-IDF 工程时使用：

```sh
idf.py set-target esp32c6
```

串口名称由 macOS 动态分配，重新插拔设备后可能发生变化。可以用以下命令重新查找：

```sh
ls /dev/cu.*
```

本次测试使用本机已安装的 ESP-IDF v5.5.5 开发版本，符合项目 README 和 ADR
规定的 `>=5.5.2,<5.6` 范围。本记录证明 USB 串口、固件构建与烧录，以及显示、
触摸、RTC、NVS 和运行中倒计时的启动级验证；完整的人手交互验收仍按对应功能票执行。

## 完整设备功能验收清单

2026-08-16 使用 ESP-IDF v5.5.5 成功构建加入 RTC 恢复、Statistics、Settings、
ES8311 提示音、AXP2101 状态、侧键和 AMOLED 生命周期管理的固件。最终
`pomodoro.bin` 为 1,397,968 字节（`0x1554d0`），应用分区仍有 56% 空余。

同日通过普通 `idf.py flash` 更新 `/dev/cu.usbmodem11101`，未执行 `erase-flash`，
所以没有擦除 `pomodoro` NVS 分区。最终串口启动日志确认 SH8601、CST9217、
ES8311 和保存状态加载完成，并进入 `Focus Ready`；持续观察 13 秒未出现 panic
或重启。目标板验证也发现并修复了大块持久化状态占用主任务/计时器任务栈的问题，
当前日志验证的是将这些工作区移出任务栈后的固件。

Agent Waiting Sound 将 Settings 格式从 9 升级到 10、顶层 persisted-state 从 6 升级到 7。
这是开发期明确接受的一次整包重置：首次启动新格式时，即使普通 flash 没有擦除 NVS，旧
Settings、运行中计时、Set Progress 与 Statistics 也会恢复默认空状态；`Waiting sound`
默认开启。

`idf.py size` 的最终静态内存结果为 DIRAM 235,346 字节（52.05%），剩余
216,766 字节；其中 `.bss` 103,664 字节、`.data` 15,700 字节。LVGL 仍使用
9,600 像素（19,200 字节）的单个局部绘制缓冲区且未启用 PSRAM。

以下项目仍需人手验收，不能视为已通过：

- 让 PCF85063 振荡停止标志保持置位，确认启动只能进入日期/时间设置；输入本地日期和
  时间后，断电重启并核对显示值与运行中计时剩余量。
- 完成跨午夜 Focus，确认完整 Pomodoro 归入自然完成时的本地 Statistics Day；连续写入
  366 个日期后确认只保留最近 365 日。
- 在 Main 与 Settings 分别按 KEY/+、BOOT/-，确认音量 0–5 同步；静音、重启、恢复声音，
  确认恢复最后一个非零音量。上电时按住 BOOT，确认下载模式行为未改变。
- 连接扬声器，确认 Focus 与 Break 完成都播放内置 `rest` 音效；静音时仍显示完成反馈且
  不发声。
- 在 Settings 的 Agent 分区确认 `Waiting sound` 位于 `Agent status` 后、默认开启、可独立
  切换并在重启后保持；`Agent status` 关闭时该行仍可编辑但不发声。分别验证首次同步、
  Working→Waiting、重启启用、十秒超时重连与确认 Error 后显露 Waiting 各播放一次内置
  `warning` 短音效；重复心跳、持续黄色、新 revision 仍为黄色、已 Waiting 时开启声音或
  解除静音均不补播。在 Main、Settings、Statistics、Numeric Stepper 与 Dialog 中验证提示，
  并确认不改变页面、计时、Set Progress 或 Statistics。制造与 Focus/Break 完成音的竞争，
  确认 Waiting 音可丢弃而完成音不丢失，且应用不进入 Audio Error。
- 电池供电、USB 供电和充电三种状态下核对电量、充电与 USB 标记；低电量提示出现一次且
  不终止 Running timer。
- PWR 短按休眠/唤醒显示，长按关机；Running 闲置 2 分钟降至所选亮度的 60%；将
  `Idle brightness` 改为 25%，确认其他状态 2 分钟后降至所选亮度的 25%、5 分钟关屏。
  调暗后点击空白区域、按钮或滑动起点均立即恢复所选亮度并从触摸时刻重新计算闲置时间；
  调暗后点击按钮或滑动仍正常执行动作且只执行一次。关屏后的第一次触摸只唤醒并消费整次
  接触，不触发点击、滑动或数值变化，第二次触摸才执行控件；重启后确认该设置仍为 25%。
- 上述两分钟调暗与五分钟关屏仅适用于非 USB Power。接入 USB 供电后，在 Ready、Paused、
  Running 与 Completion Transition 中等待超过原阈值，确认显示保持用户所选正常亮度且不熄屏；
  先让电池策略调暗或自动熄屏，再接入 USB，确认下一次状态更新立即恢复正常亮度并唤醒；
  USB 供电期间用户主动短按 PWR 熄屏保持有效且不会被周期更新唤醒；拔出 USB 后确认从拔出
  时刻重新计算闲置时间，两分钟调暗、五分钟关屏恢复；仅显示 `Charging` 而无 USB Power 时
  现有省电策略仍生效。
- 观察静态内容每分钟在 ±2 像素内偏移，确认重置/模式切换等阻塞对话框不随之移动。
- 让 PCF85063 振荡停止标志保持置位，确认首次设置直接进入不可跳过的滚轮式手动设置界面且不显示
  Back；分别从首次设置与 Settings 的 `Date & time` 进入：核对 Year/Month/Day/Hour/Minute
  五个滚轮范围（2020–2069、1–12、1–31、0–23、0–59），年月改变时日期自动收敛；
  确认五个滚轮到边界后停止且不首尾循环，快速滑动停稳后直接采用最终数字、无反复重绘卡顿；
  修改年月日时分后 Confirm 写入 RTC，首次设置进入 Main、Settings 来源返回 Settings；
  Settings 来源的 Back 丢弃草稿。首次设置阶段调节音量后浮层立即显示，并在两秒后自动消失，连续调节
  重新计时；首次设置阶段 PWR 熄屏/唤醒与关机仍可用。断电重启后核对显示值与运行中计时剩余量。
- 在导航、音频、绘制和 NVS 保存同时发生时测量 RTC deadline 到 Completion 的
  偏差；执行长时间运行、重复断电重启及更新前后逐项状态对照。
- 校准 QMI8658 到屏幕的轴向映射，记录 `0°`/`90°`/`180°`/`270°` 四向与重力分量的对应
  关系（默认按 `gx=ax, gy=ay`，必要时在 `pomodoro_board_read_acceleration` 翻转符号）。
- **已完成（2026-08-19）**：QMI8658 轴向标定通过实机四向旋转验证，传感器与屏幕坐标系
  同向（恒等映射 `gx=ax, gy=ay`，无需翻转）。实测读数与方向：顶边在上 `(gx≈0, gy≈+1)`→`0°`；
  右边在下 `(gx≈+1, gy≈0)`→`90°`；倒放 `(gx≈0, gy≈-1)`→`180°`；左边在下 `(gx≈-1, gy≈0)`→`270°`。
  修复了 `pomodoro_orientation_classify` 中 90°/270° 分支与顺时针旋转约定相反的问题。
- 在 Main、Menu、Statistics、Settings、Numeric Stepper、Dialog 与覆盖层上，
  以四个 UI Orientation 分别验证按钮与边缘手势的触摸坐标正确。
- 转动设备验证约 600ms 稳定后无动画切换、平放/对角保持当前方向、触摸或滑动中不旋转、
  释放后才应用；Rotation 期间页面、滚动位置、Dialog、Timer 状态与 deadline 保持不变。
- 熄屏期间转动后唤醒直接显示最新方向；熄屏后第一次触摸只唤醒；调光、像素偏移与完成唤醒
  在各方向均正常。
- 断开/失效 QMI8658：`Auto-rotate unavailable` 每开机最多一次、三秒自动消失、不中止计时，
  之后每五秒自动重试并在恢复后静默回到自动旋转。
- 记录软件旋转启用前后 free memory、帧时间与触摸延迟，确认只增加约 19.2KB 旋转缓冲，
  无整屏帧缓冲或双缓冲，且计时显示误差与完成时刻仍在既有的一秒要求内。
- **已完成（2026-08-19，内存部分）**：在 `lvgl_port_add_disp()` 前后实测 `MALLOC_CAP_DMA`
  可用内存。软件旋转关闭：218,416 → 199,152 B（差 19,264 B，即 19,200 B 局部绘制缓冲加少量上下文）；
  软件旋转开启：218,416 → 179,948 B（差 38,468 B，即 19,200 B 绘制缓冲 + 19,200 B 旋转缓冲）。
  旋转缓冲增量约 19.2KB，符合规格；未分配整屏帧缓冲（460,800 B）或双缓冲。帧时间与触摸延迟另列。

## 当前自动化验收命令

```sh
cmake -S test -B build/host
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
. "$HOME/esp/esp-idf/export.sh"
idf.py build
idf.py size
tools/flash.sh -p /dev/cu.usbmodem11101
idf.py -p /dev/cu.usbmodem11101 monitor
```

`tools/flash.sh` 保留 USB 端口（写入用户级 `~/.local/state/pomodoro/agent-status/flash-reservation`，或 `$XDG_STATE_HOME` 下的对应路径），等 Agent Status
broker 释放串口后再执行 `idf.py flash`，并在成功、失败、超时或中断时清理保留；
普通 `flash` 只更新 bootloader、分区表和 factory 应用地址范围；不要运行
`erase-flash`，后者会删除保存的产品状态。

Codex 个人插件通过 lifecycle hooks 向 Agent Status broker 上报；同一 Git 仓库中的
chat、fork、side chat 与 subagent 聚合为一个 Codex Workspace。执行 `$pomodoro-select`
显式选择当前 Workspace；所选 Workspace 暂时消失时保留选择并等待恢复，不自动回退。
broker 是唯一 USB 串口持有者。

## 2026-08-16 物理控制复验

实机短按 PWR 暴露出应用的每秒显示策略会覆盖用户主动休眠，表现为熄屏后立即重新
点亮。增加主动休眠状态后重新刷机，用户确认短按能够保持黑屏；第二次短按唤醒仍由
同一回归测试覆盖。KEY/+ 的官方原理图网络也由错误的 GPIO18 更正为低有效 GPIO10；
音量 0–5 的完整按键验收仍保留在上方人工检查清单中。
