// V2 plugin loader entrypoint: directory plugins resolve a literal "tui"
// subpath (tui.*) rather than package.json "exports"["./tui"], so this
// re-exports the TUI plugin definition.
export { default } from "./opencode-tui.mjs";
export * from "./opencode-tui.mjs";
