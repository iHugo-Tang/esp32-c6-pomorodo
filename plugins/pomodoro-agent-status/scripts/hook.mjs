#!/usr/bin/env node

import process from "node:process";
import { discoverHostPid, reporterSocketPath, request, spawnReporter, validateHookInput } from "./reporter.mjs";

async function readInput() {
  let data = "";
  for await (const chunk of process.stdin) {
    data += chunk;
    if (Buffer.byteLength(data) > 8 * 1024) throw new Error("hook input too large");
  }
  return validateHookInput(JSON.parse(data));
}

async function deliver(input) {
  const socketPath = reporterSocketPath(input.session_id);
  try {
    return await request(socketPath, input);
  } catch {
    spawnReporter({ sessionId: input.session_id, cwd: input.cwd, hostPid: discoverHostPid() });
    const deadline = Date.now() + 750;
    while (Date.now() < deadline) {
      await new Promise((resolve) => setTimeout(resolve, 25));
      try { return await request(socketPath, input); } catch {}
    }
    throw new Error("status source unavailable");
  }
}

try {
  const input = await readInput();
  let result;
  try {
    result = await deliver(input);
  } catch (error) {
    if (input.hook_event_name === "UserPromptSubmit" && input.prompt.trim() === "$pomodoro-select") {
      process.stdout.write(`${JSON.stringify({ hookSpecificOutput: { hookEventName: "UserPromptSubmit", additionalContext: `Pomodoro selection result: Pomodoro selection failed: ${error.message}.` } })}\n`);
    } else if (["Stop", "SubagentStop"].includes(input.hook_event_name)) {
      process.stdout.write("{}\n");
    }
    process.exit(0);
  }
  if (input.hook_event_name === "UserPromptSubmit" && input.prompt.trim() === "$pomodoro-select") {
    const message = result?.ok
      ? `Selected current Codex Workspace (revision ${result.revision}).`
      : `Pomodoro selection failed: ${result?.error || "broker not running or source unavailable"}.`;
    process.stdout.write(`${JSON.stringify({ hookSpecificOutput: { hookEventName: "UserPromptSubmit", additionalContext: `Pomodoro selection result: ${message}` } })}\n`);
  } else if (["Stop", "SubagentStop"].includes(input.hook_event_name)) {
    process.stdout.write("{}\n");
  }
} catch {
  // Lifecycle reporting is advisory and must never interrupt Codex.
}
