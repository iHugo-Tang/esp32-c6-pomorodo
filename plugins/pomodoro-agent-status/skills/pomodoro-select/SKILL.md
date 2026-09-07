---
name: pomodoro-select
description: Select the current Codex Workspace as the Pomodoro Agent Source via an explicit standalone selection command.
---

# Select the current Codex Workspace

The hook accepts only a standalone `$pomodoro-select` or
`$pomodoro-agent-status:pomodoro-select`, with optional surrounding whitespace.
For these commands, the `UserPromptSubmit` hook performs the selection and supplies
a line beginning with `Pomodoro selection result:`.

If the invocation is combined with another request, explain that selection needs
a standalone command and continue the other requested work. The result-only rules
below apply only to standalone selection commands.

- Do not call tools.
- Reply with only the result text after that prefix.
- If the prefix is absent, reply `Pomodoro selection failed: status source unavailable.`
