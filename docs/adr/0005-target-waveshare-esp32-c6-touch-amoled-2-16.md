# 以 Waveshare ESP32-C6-Touch-AMOLED-2.16 为目标硬件

产品固定运行于 Waveshare ESP32-C6-Touch-AMOLED-2.16，使用其 480×480 AMOLED、CST9220 触摸、PCF85063 RTC、ES8311 音频与 16MB Flash。该板只有 512KB HP SRAM 且没有 PSRAM，因此 LVGL 必须使用小型局部绘制缓冲，不得分配完整 RGB565 帧缓冲或全屏双缓冲。

## Consequences

圆环按秒刷新；模式切换以阈值手势加短淡入淡出代替全屏跟手拖动；完成彩带使用少量预制图形并快速停止。任何情况下都优先保障触摸响应和计时准确性，动画可继续降级。
