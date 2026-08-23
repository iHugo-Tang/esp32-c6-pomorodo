---
name: pomodoro-select
description: Select the current Codex Workspace as the Pomodoro Agent Source. Use only when the user invokes $pomodoro-select.
---

# Select the current Codex Workspace

The `UserPromptSubmit` hook already performed the selection and supplied a line beginning with `Pomodoro selection result:`.

- Do not call tools.
- Reply with only the result text after that prefix.
- If the prefix is absent, reply `Pomodoro selection failed: status source unavailable.`
