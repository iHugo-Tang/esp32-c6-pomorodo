# Agent instructions

## Working conventions

- Treat requests such as "can you", "help me", and "I want to" as instructions
  to carry out the work. Continue through implementation and relevant verification.
- Proceed with authorized, reversible work without repeated approval. If an
  action requires approval, first prepare the concrete, reviewable result and
  complete any independent work. Do not infer authorization for destructive or
  irreversible actions.
- Keep changes within the requested scope and preserve unrelated user changes.
- Do not turn optional skill guidance into an approval requirement. If a skill
  blocks completion, link and quote the instruction that requires the pause.
- Use subagents only when the user or applicable instructions authorize them.
  When authorized, delegate bounded, independent work with clear ownership;
  review and integrate the results before reporting completion.

## Repository context

### Issue tracker

Issues and specs are tracked as local Markdown under `_scratch/`. See `docs/agents/issue-tracker.md`.

### Triage labels

Use the five canonical triage roles with their default names. See `docs/agents/triage-labels.md`.

### Domain docs

For changes involving product behavior, domain terminology, or architecture,
consult `CONTEXT.md` and applicable ADRs, using `docs/adr/README.md` to check
their status. See `docs/agents/domain.md` for domain conventions.

### UI stack

The device UI is C code built with ESP-IDF and LVGL 9. For UI work, follow
the LVGL architecture in `docs/adr/0004-use-esp-idf-and-lvgl.md` and
`docs/product/firmware.md`. Do not load or apply SwiftUI conventions unless
SwiftUI code is explicitly introduced into the repository.

## Verification and completion

- Use documented verification commands in `README.md` and relevant component
  docs. Low-impact documentation edits do not need firmware builds or new tests.
- Once relevant checks pass, broaden or repeat them only when new changes,
  failures, or unresolved concerns justify it.
- Firmware builds and host tests do not prove physical-device behavior; state
  when hardware validation remains.
