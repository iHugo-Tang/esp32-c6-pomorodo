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
