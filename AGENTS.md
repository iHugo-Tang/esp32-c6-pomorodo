## Agent skills

### Issue tracker

Issues and specs are tracked as local Markdown under `_scratch/`. See `docs/agents/issue-tracker.md`.

### Triage labels

Use the five canonical triage roles with their default names. See `docs/agents/triage-labels.md`.

### Domain docs

This is a single-context repository. Read the root `CONTEXT.md` and relevant ADRs under `docs/adr/`. See `docs/agents/domain.md`.

### UI stack

The device UI is C code built with ESP-IDF and LVGL 9. For UI work, follow
the LVGL architecture in `docs/adr/0004-use-esp-idf-and-lvgl.md` and
`docs/product/firmware.md`. Do not load or apply SwiftUI conventions unless
SwiftUI code is explicitly introduced into the repository.
