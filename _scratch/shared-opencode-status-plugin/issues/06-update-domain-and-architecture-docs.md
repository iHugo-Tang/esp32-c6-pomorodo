# 06 — Domain and architecture documentation

**What to build:** Recording the user-level plugin and process-selection design in the project's domain and architecture documentation: add OpenCode Process and Selected OpenCode Process to the glossary, update the OpenCode Status definition, add an ADR that supersedes the project-plugin, single-process, and project-level reservation statements in ADR 0010, and keep README, the hardware connection test notes, and the superseded OpenCode Status spec consistent with the new model.

**Blocked by:** 03 — Add the pomodoro-select command; 04 — Migrate the flash reservation to user level; 05 — Remove the project-level plugin.

**Status:** ready-for-agent

- [x] The glossary gains OpenCode Process (one running OpenCode instance with all sessions of its current project) and Selected OpenCode Process (the single process currently providing the aggregate status to the device), and the OpenCode Status entry reflects single-source selection with explicit reselection and automatic fallback.
- [x] A new ADR records the user-level plugin, explicit selection plus automatic fallback, the shared global revision, and the user-level flashing reservation, and explicitly supersedes the superseded statements in ADR 0010.
- [x] README and the hardware connection test notes describe the user-level plugin path, `/pomodoro-select`, and the user-level reservation without referencing the removed project plugin files.
- [x] The superseded OpenCode Status spec and its tickets note the parts replaced by this feature.
