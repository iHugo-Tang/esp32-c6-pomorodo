---
status: superseded by ADR-0012 and ADR-0015
---

# 用户级 OpenCode Status 插件与所选进程协调

为让任意项目中的 OpenCode 都能显示状态，插件曾从项目目录迁到用户级配置。多个 OpenCode 进程在共享状态目录注册，通过显式选择和自动回退选出唯一串口所有者；revision 与 `flash-reservation` 也放在共享目录。

## Consequences

- Pomodoro 仓库不再保留项目级 OpenCode 插件，避免重复加载。
- 只有所选进程打开串口并发送状态；其他进程只维护自己的聚合结果。
- 存储不可用时 fail closed，不连接设备、不参与选择。
- 烧录保留期间所有实例释放串口，结束后恢复。

这个宿主进程竞选方案被独立 broker 取代；OpenCode 来源随后被彻底移除。
