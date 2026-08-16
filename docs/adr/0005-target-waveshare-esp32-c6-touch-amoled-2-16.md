# 以 Waveshare ESP32-C6-Touch-AMOLED-2.16 为目标硬件

产品固定运行于 Waveshare ESP32-C6-Touch-AMOLED-2.16，使用其 480×480 SH8601 QSPI AMOLED、CST9217 触摸、PCF85063 RTC、ES8311 音频与 16MB Flash。该板只有 512KB HP SRAM 且没有 PSRAM，因此 LVGL 必须使用小型局部绘制缓冲，不得分配完整 RGB565 帧缓冲或全屏双缓冲。

## 2026-08-15 硬件标识更正

初版 ADR 将显示与触摸控制器误记为 CO5300/CST9220。实现前对照 [Waveshare 官方 ESP32-C6-Touch-AMOLED-2.16 仓库](https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16) 的 ESP-IDF 5.5.3 与 XiaoZhi 2.2.5 板级代码，并在实机启动时识别到 SH8601 1.0.2 与 CST9217 驱动，因此将决定更正为 SH8601 QSPI 与 CST9217。此更正不改变既有的 480×480、无 PSRAM及局部缓冲约束。

## Consequences

圆环按秒刷新；模式切换以阈值手势加短淡入淡出代替全屏跟手拖动；完成彩带使用少量预制图形并快速停止。任何情况下都优先保障触摸响应和计时准确性，动画可继续降级。
