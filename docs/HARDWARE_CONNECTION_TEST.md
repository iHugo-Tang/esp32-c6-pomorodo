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
ES8311 提示音、AXP2101 状态、侧键、AMOLED 生命周期管理和 iPhone 本地校时的
固件。最终 `pomodoro.bin` 为 1,397,968 字节（`0x1554d0`），应用分区仍有 56%
空余。

同日通过普通 `idf.py flash` 更新 `/dev/cu.usbmodem11101`，未执行 `erase-flash`，
所以没有擦除 `pomodoro` NVS 分区。最终串口启动日志确认 SH8601、CST9217、
ES8311 和保存状态加载完成，并进入 `Focus Ready`；持续观察 13 秒未出现 panic
或重启。目标板验证也发现并修复了大块持久化状态占用主任务/计时器任务栈的问题，
当前日志验证的是将这些工作区移出任务栈后的固件。

`idf.py size` 的最终静态内存结果为 DIRAM 235,346 字节（52.05%），剩余
216,766 字节；其中 `.bss` 103,664 字节、`.data` 15,700 字节。LVGL 仍使用
9,600 像素（19,200 字节）的单个局部绘制缓冲区且未启用 PSRAM。

以下项目仍需人手验收，不能视为已通过：

- 让 PCF85063 振荡停止标志保持置位，确认启动只能进入日期/时间设置；输入本地日期、
  时间和 UTC 固定偏移后，断电重启并核对显示值与运行中计时剩余量。
- 完成跨午夜 Focus，确认完整 Pomodoro 归入自然完成时的本地 Statistics Day；连续写入
  366 个日期后确认只保留最近 365 日。
- 在 Main 与 Settings 分别按 KEY/+、BOOT/-，确认音量 0–5 同步；静音、重启、恢复声音，
  确认恢复最后一个非零音量。上电时按住 BOOT，确认下载模式行为未改变。
- 连接扬声器，确认 Focus 完成播放两段短促上升音，Break 完成仅播放一段柔和音；静音时
  仍显示完成反馈且不发声。
- 电池供电、USB 供电和充电三种状态下核对电量、充电与 USB 标记；低电量提示出现一次且
  不终止 Running timer。
- PWR 短按休眠/唤醒显示，长按关机；Running 闲置 2 分钟降至所选亮度的 60%，其他状态
  2 分钟降至 20%、5 分钟关屏。关屏后的第一次触摸只唤醒，第二次才执行控件。
- 观察静态内容每分钟在 ±2 像素内偏移，确认重置/模式切换等阻塞对话框不随之移动。
- 从首次设置和 Settings 分别启动 `Sync from iPhone`：扫描随机 WPA2 QR、打开
  `192.168.4.1`，确认页面明确提交后 RTC/时区才改变；分别观察成功、取消、非法
  数据和 5 分钟超时后 SoftAP 消失，且设备没有加入路由器或保留凭据。
- 在导航、音频、绘制和 NVS 保存同时发生时测量 RTC deadline 到 Completion 的
  偏差；执行长时间运行、重复断电重启及更新前后逐项状态对照。

## 当前自动化验收命令

```sh
cmake -S test -B build/host
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
. "$HOME/esp/esp-idf/export.sh"
idf.py build
idf.py size
idf.py -p /dev/cu.usbmodem11101 flash
idf.py -p /dev/cu.usbmodem11101 monitor
```

普通 `flash` 只更新 bootloader、分区表和 factory 应用地址范围；不要运行
`erase-flash`，后者会删除保存的产品状态。

## 2026-08-16 物理控制复验

实机短按 PWR 暴露出应用的每秒显示策略会覆盖用户主动休眠，表现为熄屏后立即重新
点亮。增加主动休眠状态后重新刷机，用户确认短按能够保持黑屏；第二次短按唤醒仍由
同一回归测试覆盖。KEY/+ 的官方原理图网络也由错误的 GPIO18 更正为低有效 GPIO10；
音量 0–5 的完整按键验收仍保留在上方人工检查清单中。
