# Issue tracker: Local Markdown

Issues and specs for this repository live as Markdown files in `_scratch/`.

## Conventions

- One feature per directory: `_scratch/<feature-slug>/`.
- The spec is `_scratch/<feature-slug>/spec.md`.
- Implementation issues are one file per ticket under `_scratch/<feature-slug>/issues/`, numbered from `01` in dependency order.
- Triage state is recorded as a `Status:` line near the top of each issue file.
- Comments and conversation history are appended under a `## Comments` heading.

When a skill says to publish to the issue tracker, create the corresponding file under `_scratch/<feature-slug>/`.

For wayfinding efforts, use `_scratch/<effort>/map.md` for the map and `_scratch/<effort>/issues/<NN>-<slug>.md` for child tickets. Child tickets record `Type:`, `Status:`, and `Blocked by:` near the top.
