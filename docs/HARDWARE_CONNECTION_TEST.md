# 硬件连接与实机验证

这份文档只记录已经观察到的硬件结果，并明确区分启动级验证和人工交互验收。

## 测试环境

2026-08-15 至 2026-08-16，在 macOS 上连接 Waveshare ESP32-C6-Touch-AMOLED-2.16：

| 项目 | 结果 |
| --- | --- |
| 串口 | `/dev/cu.usbmodem11101`，实际名称可能随插拔变化 |
| 芯片 | ESP32-C6 QFN40，revision v0.2 |
| USB | USB-Serial/JTAG |
| ESP-IDF | v5.5.5，符合 `>=5.5.2,<5.6` |
| esptool | v4.12.0 |

## 已验证

- `esptool` 能识别芯片、上传 stub 并复位；最初的连通性测试没有写 Flash。
- 普通 flash 成功，没有执行 `erase-flash`。
- SH8601、CST9217、PCF85063、ES8311 和 NVS 状态在启动时初始化成功。
- 屏幕分辨率为 480×480；LVGL 使用 9,600 像素、19,200 字节的单个局部绘制缓冲，无 PSRAM。
- 恢复后的 Running Timer 按绝对截止时间每秒递减。
- 固件进入 `Focus Ready` 后连续观察 13 秒，没有 panic 或重启。
- 当时构建的 `pomodoro.bin` 为 1,397,968 字节，应用分区剩余 56%。
- DIRAM 使用 235,346 字节，剩余 216,766 字节。

这些结果证明设备能连接、构建、烧录和启动，也证明主要驱动能初始化。它们不等于完整的人手交互验收。

## 仍需明确验收记录

- PCF85063 失效、恢复和 Pending Statistics 的完整实机流程；
- 物理音量键、电池、充电、USB Power 和 PMIC 行为；
- AMOLED 调暗、Display Off、触摸唤醒、完成唤醒和像素偏移；
- 日期时间滚轮快速滑动的流畅度和停稳值；
- 长时间运行、跨午夜和重复断电恢复的计时误差。

只有 issue 中记录了明确的测试时间、操作和结果，才能把对应验收视为完成。

## 自动化与构建

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

串口名称由 macOS 动态分配，可用 `ls /dev/cu.usbmodem*` 重新查找。ESP32-C6 没有传统 Chip ID；`esptool` 使用 MAC 作为设备标识时出现相关提示是正常现象。

## 安全烧录

使用 `tools/flash.sh`；烧录前关闭占用串口的 monitor。

不要运行 `erase-flash`。它会删除 Settings、当前 Timer、Set Progress 和 Statistics。正常 flash 只更新固件相关分区并保留产品状态。
