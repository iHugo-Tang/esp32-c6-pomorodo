#!/usr/bin/env node

import assert from "node:assert/strict";
import { chmod, mkdtemp, mkdir, realpath, stat } from "node:fs/promises";
import net from "node:net";
import os from "node:os";
import path from "node:path";
import process from "node:process";
import { BrokerCore, runBroker } from "../../../tools/pomodoro-agent-status-broker.mjs";
import {
  MASK, StatusState, brokerSocketPath, discoverHostPid, hashId, reporterSocketPath,
  request, startReporter, structuredFailure, validateHookInput, workspaceRoot,
} from "./reporter.mjs";

let checks = 0;
function pass(name) { console.log(`ok ${++checks}: ${name}`); }
function hook(event, fields = {}) {
  return { session_id: "session-a", cwd: process.cwd(), hook_event_name: event, ...fields };
}
async function waitFor(test, timeout = 2_000) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) {
    if (await test()) return;
    await new Promise((resolve) => setTimeout(resolve, 20));
  }
  throw new Error("timed out waiting for condition");
}

async function testPureCore() {
  assert.equal(hashId("x").length, 64);
  assert.equal(hashId("x"), hashId("x"));
  const temporary = await mkdtemp(path.join(os.tmpdir(), "pomodoro-workspace-"));
  const child = path.join(temporary, "child");
  await mkdir(child);
  assert.equal(await workspaceRoot(child, () => temporary), await realpath(temporary));
  assert.equal(await workspaceRoot(child, () => { throw new Error("not git"); }), await realpath(child));
  assert.match(reporterSocketPath("session-a"), /^\/tmp\/pomodoro-codex-\d+\/[a-f0-9]{24}\.sock$/);
  assert.equal(brokerSocketPath({ HOME: "/home/a" }), "/home/a/.local/state/opencode/pomodoro-status/agent-status.sock");
  assert.throws(() => validateHookInput({}));
  assert.throws(() => validateHookInput(hook("UserPromptSubmit", { prompt: 1 })));
  assert.equal(validateHookInput(hook("UserPromptSubmit", { prompt: "hello", extension: true })).prompt, "hello");
  assert.equal(structuredFailure({ output: { exit_code: 1 } }), true);
  assert.equal(structuredFailure({ content: [{ isError: true }] }), true);
  assert.equal(structuredFailure({ exit_code: 0 }), false);
  const processes = new Map([[9, { ppid: 8, command: "/bin/sh" }], [8, { ppid: 1, command: "/usr/local/bin/codex" }]]);
  assert.equal(discoverHostPid(9, (pid) => processes.get(pid)), 8);
  pass("workspace identity, hashes, strict hook input, failures, and Codex ancestor discovery");

  const state = new StatusState();
  state.apply(hook("SessionStart", { source: "startup" }));
  assert.equal(state.main, MASK.IDLE);
  state.apply(hook("UserPromptSubmit", { prompt: "work" }));
  assert.equal(state.main, MASK.WORKING);
  state.apply(hook("PermissionRequest", { tool_name: "Bash" }));
  assert.equal(state.main, MASK.WAITING);
  state.apply(hook("PreToolUse", { tool_name: "request_user_input" }));
  assert.equal(state.main, MASK.WAITING);
  state.apply(hook("PostToolUse", { tool_name: "Bash", tool_response: { exit_code: 2 } }));
  state.apply(hook("Stop"));
  assert.equal(state.main, MASK.ERROR, "error remains latched through Stop");
  state.apply(hook("PreToolUse", { tool_name: "Bash" }));
  state.apply(hook("PostToolUse", { tool_name: "Bash", tool_response: { exit_code: 0 } }));
  state.apply(hook("Stop"));
  assert.equal(state.main, MASK.COMPLETED);
  state.apply(hook("SessionStart", { source: "compact" }));
  assert.equal(state.main, MASK.COMPLETED, "compact does not reset state");
  assert.equal(state.apply(hook("UserPromptSubmit", { prompt: "  $pomodoro-select  " })).control, true);
  assert.equal(state.main, MASK.COMPLETED, "control turn does not change state");
  state.apply(hook("SubagentStart", { agent_id: "sub", agent_type: "worker" }));
  assert.equal(state.subagents.get("sub"), MASK.WORKING);
  state.apply(hook("SubagentStop", { agent_id: "sub", agent_type: "worker" }));
  assert.equal(state.subagents.size, 0);
  pass("five-state mapping, error latch, compact, control turn, and subagents");
}

function brokerRequest(socketPath, value) { return request(socketPath, value, 1_000); }

async function testReporterIntegration() {
  const root = await mkdtemp("/tmp/pomodoro-reporter-");
  const stateDir = path.join(root, "opencode", "pomodoro-status");
  const broker = await runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
  const env = { HOME: root, XDG_STATE_HOME: root };
  const sessionA = `a-${process.pid}-${Date.now()}`;
  const sessionB = `b-${process.pid}-${Date.now()}`;
  let hostAlive = true;
  let reporterA;
  let reporterB;
  try {
    reporterA = await startReporter({ sessionId: sessionA, cwd: process.cwd(), hostPid: process.pid, env, intervalMs: 80 });
    reporterB = await startReporter({ sessionId: sessionB, cwd: process.cwd(), hostPid: process.pid, env, intervalMs: 80, isAlive: () => hostAlive });
    await waitFor(() => broker.core.members.size === 1 && [...broker.core.members.values()][0].size === 2);
    assert.equal((await stat(path.dirname(reporterA.localSocket))).mode & 0o777, 0o700);
    assert.equal((await stat(reporterA.localSocket)).mode & 0o777, 0o600);

    await brokerRequest(reporterA.localSocket, hook("UserPromptSubmit", { session_id: sessionA, prompt: "work" }));
    await brokerRequest(reporterB.localSocket, hook("SubagentStart", { session_id: sessionB, agent_id: "sub", agent_type: "worker" }));
    await waitFor(() => broker.core.publication()?.mask === MASK.WORKING);
    assert.equal([...broker.core.members.values()][0].size, 3, "sessions and subagents aggregate in one Workspace");
    await brokerRequest(reporterB.localSocket, hook("SubagentStop", { session_id: sessionB, agent_id: "sub", agent_type: "worker" }));
    await waitFor(() => [...broker.core.members.values()][0].size === 2);

    const before = broker.core.revision;
    const selected = await brokerRequest(reporterA.localSocket, hook("UserPromptSubmit", { session_id: sessionA, prompt: "$pomodoro-select" }));
    assert.equal(selected.ok, true);
    assert.equal(selected.revision, before + 1, "control command creates exactly one revision");
    assert.equal(reporterA.state.main, MASK.WORKING, "selection does not add a status cycle");
    await brokerRequest(reporterA.localSocket, hook("SessionEnd", { session_id: sessionA, reason: "other" }));
    await waitFor(() => [...broker.core.members.values()][0].size === 1);
    hostAlive = false;
    await waitFor(() => broker.core.members.size === 0);
    pass("reporter permissions, lease renewal, Workspace aggregation, selection, SessionEnd, and host-death cleanup");
  } finally {
    await reporterA?.close();
    await reporterB?.close();
    await broker.close();
  }

  const isolated = new BrokerCore({ isAlive: () => true });
  await isolated.handle({ type: "report", client: "codex", groupId: "one", memberId: "a", pid: 1, startedAt: 1, mask: 4 });
  await isolated.handle({ type: "report", client: "codex", groupId: "two", memberId: "b", pid: 1, startedAt: 1, mask: 1 });
  assert.equal(isolated.publication().mask, 4);
  pass("different Workspaces remain isolated");
}

async function testBrokerRecoveryAndFailures() {
  const root = await mkdtemp("/tmp/pomodoro-recovery-");
  const stateDir = path.join(root, "opencode", "pomodoro-status");
  const env = { HOME: root, XDG_STATE_HOME: root };
  const sessionId = `recovery-${process.pid}-${Date.now()}`;
  let reporter;
  let broker;
  try {
    reporter = await startReporter({ sessionId, cwd: process.cwd(), hostPid: process.pid, env, intervalMs: 60 });
    await request(reporter.localSocket, hook("UserPromptSubmit", { session_id: sessionId, prompt: "work" }));
    broker = await runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
    await waitFor(() => broker.core.publication()?.mask === MASK.WORKING);
    console.log("# recovery: initial broker connected");
    await broker.close();
    broker = null;
    console.log("# recovery: initial broker closed");

    const corruptSockets = new Set();
    const corrupt = net.createServer((socket) => {
      corruptSockets.add(socket);
      socket.once("close", () => corruptSockets.delete(socket));
      socket.end("not-json\n");
    });
    await new Promise((resolve) => corrupt.listen(path.join(root, "bad.sock"), resolve));
    await assert.rejects(request(path.join(root, "bad.sock"), { ping: true }), /invalid response/);
    for (const socket of corruptSockets) socket.destroy();
    await new Promise((resolve) => corrupt.close(resolve));
    console.log("# recovery: malformed response rejected");

    broker = await runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
    await waitFor(() => broker.core.publication()?.mask === MASK.WORKING);
    pass("missing broker, malformed responses, and broker restart do not lose reporter state");
  } finally {
    await reporter?.close();
    await broker?.close();
  }
}

await testPureCore();
await testReporterIntegration();
await testBrokerRecoveryAndFailures();
console.log(`pomodoro-agent-status check: ${checks} checks passed`);
