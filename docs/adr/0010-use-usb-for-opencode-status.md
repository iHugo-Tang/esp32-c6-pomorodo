# 通过 USB 双向同步 OpenCode Status

设备与当前项目的 OpenCode 插件通过现有 USB Serial/JTAG 连接交换短行协议，不为状态指示器启用 Wi-Fi、BLE 或常驻网络服务。USB 已由目标硬件与本地工作流验证，能在不引入配网、凭据和新依赖的情况下同时承载 OpenCode Status、固件刷写与调试；插件占用串口时，串口监视器或刷写工具需暂时取得独占访问，插件静默断开并在端口释放后重连。

## Consequences

- 固件将 USB Serial/JTAG 作为可输入的主控制台，并严格校验带版本、长度受限的状态行；普通日志不是协议输入。
- 设备周期发送 `ENABLED` 或 `DISABLED` 控制状态。关闭 `OpenCode status` 时插件停止心跳，设备不读取 OpenCode 状态；重新开启后插件立即同步。
- 插件每三秒发送当前项目的聚合状态心跳；设备十秒未收到有效心跳就隐藏指示器。
- 该通道只同步临时显示状态，不远程控制 Pomodoro、不写入统计数据，也不扩展为 OTA 通道。
