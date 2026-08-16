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
本测试只读取芯片信息，没有擦除或写入 Flash。

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

本次连通性测试使用本机已安装的 ESP-IDF v5.5.5 开发版本，符合项目
README 和 ADR 规定的 `>=5.5.2,<5.6` 范围。本记录只证明 USB 串口通信和芯片识别成功，
不代表固件已经完成构建、烧录或板载外设验证。
