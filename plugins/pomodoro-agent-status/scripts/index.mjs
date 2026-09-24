// V2 plugin loader entrypoint: directory plugins resolve index.* rather than
// package.json "main", so this re-exports the server plugin definition.
export { default } from "./opencode-server.mjs";
export * from "./opencode-server.mjs";
