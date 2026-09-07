#!/usr/bin/env node

import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { chmod, mkdtemp, mkdir, readFile, realpath, stat, writeFile } from "node:fs/promises";
import net from "node:net";
import os from "node:os";
import path from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";
import { BrokerCore, MEMBER_LEASE_MS, runBroker } from "./broker.mjs";
import {
  MASK, StatusState, brokerSocketPath, discoverHostPid, ensureBroker, hashId, isSelectionPrompt, reporterSocketPath,
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
  assert.equal(brokerSocketPath({ HOME: "/home/a" }), "/home/a/.local/state/pomodoro/agent-status/agent-status.sock");
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
  assert.equal(state.apply(hook("UserPromptSubmit", { prompt: "$pomodoro-agent-status:pomodoro-select" })).control, true);
  assert.equal(state.main, MASK.COMPLETED, "namespaced control turn does not change state");
  state.apply(hook("SubagentStart", { agent_id: "sub", agent_type: "worker" }));
  assert.equal(state.subagents.get("sub"), MASK.WORKING);
  state.apply(hook("SubagentStop", { agent_id: "sub", agent_type: "worker" }));
  assert.equal(state.subagents.size, 0);
  pass("five-state mapping, error latch, compact, control turn, and subagents");
}

function brokerRequest(socketPath, value) { return request(socketPath, value, 1_000); }

async function runHook(input, env) {
  const child = spawn(process.execPath, [fileURLToPath(new URL("./hook.mjs", import.meta.url))], {
    env, stdio: ["pipe", "pipe", "inherit"],
  });
  let output = "";
  child.stdout.setEncoding("utf8");
  child.stdout.on("data", (chunk) => { output += chunk; });
  child.stdin.end(JSON.stringify(input));
  const code = await new Promise((resolve, reject) => {
    child.once("error", reject);
    child.once("close", resolve);
  });
  assert.equal(code, 0);
  return output;
}

async function testHookSelection() {
  const root = await mkdtemp("/tmp/pomodoro-hook-");
  const stateDir = path.join(root, "pomodoro", "agent-status");
  const env = { ...process.env, XDG_STATE_HOME: root, PLUGIN_ROOT: fileURLToPath(new URL("..", import.meta.url)) };
  const sessionId = `hook-${process.pid}-${Date.now()}`;
  const broker = await runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
  const reporter = await startReporter({ sessionId, cwd: process.cwd(), hostPid: process.pid, env, intervalMs: 80 });
  try {
    await waitFor(() => broker.core.members.size === 1);
    const before = broker.core.revision;
    const input = hook("UserPromptSubmit", { session_id: sessionId, prompt: "$pomodoro-agent-status:pomodoro-select" });
    assert.equal(isSelectionPrompt(input), true);
    const output = JSON.parse(await runHook(input, env));
    assert.match(output.hookSpecificOutput.additionalContext, /^Pomodoro selection result: Selected current Codex Workspace \(revision \d+\)\.$/);
    assert.equal(broker.core.revision, before + 1, "hook selection creates exactly one revision");
    assert.equal(reporter.state.main, MASK.IDLE, "hook control turn does not add a status cycle");
    pass("real namespaced hook selects the Workspace and returns one revision");
  } finally {
    await reporter.close();
    await broker.close();
  }
}

async function testReporterIntegration() {
  const root = await mkdtemp("/tmp/pomodoro-reporter-");
  const stateDir = path.join(root, "pomodoro", "agent-status");
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
  await isolated.handle({ type: "report", groupId: "one", memberId: "a", pid: 1, startedAt: 1, mask: 4 });
  await isolated.handle({ type: "report", groupId: "two", memberId: "b", pid: 1, startedAt: 1, mask: 1 });
  assert.equal(isolated.publication().mask, 4);
  pass("different Workspaces remain isolated");
}

async function testOpenCodeActivation() {
  const root = await mkdtemp("/tmp/pomodoro-opencode-");
  const stateDir = path.join(root, "pomodoro", "agent-status");
  const groupId = hashId("/same/normalized/repo/root");
  const startedAt = Date.now();
  const codexReport = { type: "report", groupId, memberId: "codex-main", pid: process.pid, startedAt, mask: MASK.WAITING };
  const opencodeReport = { type: "report", groupId, memberId: "opencode-main", pid: process.pid, startedAt, mask: MASK.IDLE, host: "opencode" };
  let broker = await runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
  try {
    await brokerRequest(broker.socketPath, opencodeReport);
    await brokerRequest(broker.socketPath, { ...opencodeReport, memberId: "opencode-sub", mask: MASK.WORKING });
    assert.equal(broker.core.selected, null, "OpenCode reports before activation do not select a source");
    assert.equal(broker.core.revision, 0, "OpenCode reports before activation do not advance the revision");
    assert.equal(broker.core.publication(), null);

    const invalid = [
      { type: "select", groupId, host: "opencode", extra: true },
      { type: "select", groupId, host: "cursor" },
      { type: "select", groupId, host: 9 },
      { type: "select", groupId, host: null },
      { type: "select", host: "opencode" },
      { type: "activate", groupId, host: "opencode" },
      { ...opencodeReport, unexpected: 1 },
      { type: "report", groupId, memberId: "x", pid: process.pid, startedAt, host: "opencode" },
      { type: "report", groupId: "g".repeat(200), memberId: "x", pid: process.pid, startedAt, mask: MASK.IDLE, host: "opencode" },
    ];
    for (const bad of invalid) {
      const reply = await brokerRequest(broker.socketPath, bad);
      assert.equal(reply.ok, false, `invalid activation data is rejected: ${JSON.stringify(bad)}`);
    }
    const oversized = { type: "select", groupId: groupId + "x".repeat(9 * 1024), host: "opencode" };
    assert.match((await request(broker.socketPath, oversized, 1_000)).error, /8 KiB/);
    assert.equal(broker.core.selected, null, "rejections leave selection unchanged");
    assert.equal(broker.core.revision, 0, "rejections leave the revision unchanged");

    const codexSelected = await brokerRequest(broker.socketPath, codexReport);
    assert.equal(codexSelected.ok, true);
    assert.deepEqual(broker.core.publication(), { revision: 1, mask: MASK.WAITING },
      "Codex first-valid-source still selects the same groupId");

    await brokerRequest(broker.socketPath, { ...opencodeReport, mask: MASK.ERROR });
    assert.equal(broker.core.selected, `codex:${groupId}`,
      "the Codex source and the OpenCode source with the same groupId stay distinct");
    assert.deepEqual(broker.core.publication(), { revision: 1, mask: MASK.WAITING },
      "unselected OpenCode reports cannot change the selected revision or mask");

    const activated = await brokerRequest(broker.socketPath, { type: "select", groupId, host: "opencode" });
    assert.equal(activated.ok, true);
    assert.equal(activated.selected, `opencode:${groupId}`, "activation addresses the host-qualified source");
    assert.equal(activated.revision, 2, "first activation creates exactly one new revision");
    assert.deepEqual(broker.core.publication(), { revision: 2, mask: MASK.ERROR | MASK.WORKING },
      "the selected OpenCode aggregate excludes the Codex source with the same groupId");

    const reactivated = await brokerRequest(broker.socketPath, { type: "select", groupId, host: "opencode" });
    assert.equal(reactivated.ok, true);
    assert.equal(reactivated.revision, 3, "reactivation creates exactly one new revision");

    const [firstAttempt, secondAttempt] = await Promise.all([
      brokerRequest(broker.socketPath, { type: "select", groupId, host: "opencode" }),
      brokerRequest(broker.socketPath, { type: "select", groupId, host: "opencode" }),
    ]);
    assert.equal(firstAttempt.ok && secondAttempt.ok, true);
    assert.equal(Math.abs(firstAttempt.revision - secondAttempt.revision), 1,
      "concurrent activation attempts stay serialized");
    assert.equal(broker.core.revision, 5, "each concurrent activation creates exactly one new revision");

    await brokerRequest(broker.socketPath, { type: "remove", groupId, memberId: "opencode-sub", host: "opencode" });
    assert.deepEqual(broker.core.publication(), { revision: 6, mask: MASK.ERROR },
      "member removal addresses the host-qualified source");

    const codexExplicit = await brokerRequest(broker.socketPath, { type: "select", groupId });
    assert.equal(codexExplicit.ok, true);
    assert.deepEqual(broker.core.publication(), { revision: 7, mask: MASK.WAITING },
      "explicit Codex selection excludes the OpenCode aggregate");

    const oddGroupId = `opencode:${groupId}`;
    const oddReport = { type: "report", groupId: oddGroupId, memberId: "codex-odd", pid: process.pid, startedAt, mask: MASK.WORKING };
    const oddReported = await brokerRequest(broker.socketPath, oddReport);
    assert.equal(oddReported.ok, true, "previously valid Codex groupIds remain valid");
    assert.deepEqual(broker.core.publication(), { revision: 7, mask: MASK.WAITING },
      "the odd Codex source is unselected and cannot change publication");
    const oddSelected = await brokerRequest(broker.socketPath, { type: "select", groupId: oddGroupId });
    assert.equal(oddSelected.ok, true);
    assert.equal(oddSelected.selected, `codex:${oddGroupId}`);
    assert.deepEqual(broker.core.publication(), { revision: 8, mask: MASK.WORKING },
      "a Codex groupId with an opencode: prefix neither collides with nor aggregates the OpenCode source");

    await brokerRequest(broker.socketPath, { type: "select", groupId, host: "opencode" });
    assert.deepEqual(broker.core.publication(), { revision: 9, mask: MASK.ERROR });
    pass("host-qualified OpenCode activation, isolation, validation, and reactivation through the broker boundary");
  } finally {
    await broker.close();
  }

  const persisted = JSON.parse(await readFile(path.join(stateDir, "agent-coordination.json"), "utf8"));
  assert.deepEqual(Object.keys(persisted).sort(), ["revision", "selected", "selectedHost"]);
  assert.equal(persisted.selected, groupId);
  assert.equal(persisted.selectedHost, "opencode");
  assert.equal(persisted.revision, 9);

  broker = await runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
  try {
    await brokerRequest(broker.socketPath, opencodeReport);
    assert.deepEqual(broker.core.publication(), { revision: 10, mask: MASK.IDLE },
      "the host-qualified selection resumes after broker restart");
    await brokerRequest(broker.socketPath, codexReport);
    assert.deepEqual(broker.core.publication(), { revision: 10, mask: MASK.IDLE },
      "Codex reports cannot take over a selected OpenCode Workspace");
    pass("persisted host-qualified selection recovers across broker restart without fallback");
  } finally {
    await broker.close();
  }
}

async function testUnselectedExpiryIsolation() {
  const root = await mkdtemp("/tmp/pomodoro-expiry-");
  const stateDir = path.join(root, "pomodoro", "agent-status");
  const groupId = hashId("/expiry/repo");
  const startedAt = Date.now();
  const broker = await runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
  try {
    broker.core.isAlive = () => true;
    await brokerRequest(broker.socketPath, { type: "report", groupId, memberId: "codex-a", pid: 111, startedAt, mask: MASK.WORKING });
    await brokerRequest(broker.socketPath, { type: "report", groupId, memberId: "codex-b", pid: 222, startedAt, mask: MASK.WAITING });
    await brokerRequest(broker.socketPath, { type: "report", groupId, memberId: "oc-main", pid: 333, startedAt, mask: MASK.IDLE, host: "opencode" });
    assert.deepEqual(broker.core.publication(), { revision: 2, mask: MASK.WORKING | MASK.WAITING });

    broker.core.isAlive = (pid) => pid !== 222;
    const before = broker.core.publication();
    await brokerRequest(broker.socketPath, { type: "report", groupId, memberId: "oc-main", pid: 333, startedAt, mask: MASK.ERROR, host: "opencode" });
    assert.deepEqual(broker.core.publication(), before,
      "an unselected OpenCode report cannot trigger the selected source's expiry advance");
    await waitFor(() => broker.core.publication()?.revision === before.revision + 1);
    assert.deepEqual(broker.core.publication(), { revision: before.revision + 1, mask: MASK.WORKING },
      "the background reaper still advances the selected source when a member expires");
    assert.equal(broker.core.selected, `codex:${groupId}`, "selection is unchanged by the unselected report");
    pass("unselected reports cannot trigger selected-source expiry advances; the background reaper owns them");
  } finally {
    await broker.close();
  }
}

async function testLegacyPersistedSelection() {
  const root = await mkdtemp("/tmp/pomodoro-legacy-");
  const stateDir = path.join(root, "pomodoro", "agent-status");
  await mkdir(stateDir, { recursive: true, mode: 0o700 });
  const groupId = "opencode:legacy-codex-workspace";
  await writeFile(path.join(stateDir, "agent-coordination.json"),
    `${JSON.stringify({ selected: groupId, revision: 7 })}\n`, { mode: 0o600 });
  const broker = await runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
  try {
    await brokerRequest(broker.socketPath, { type: "report", groupId, memberId: "codex-main", pid: process.pid, startedAt: Date.now(), mask: MASK.WAITING });
    assert.deepEqual(broker.core.publication(), { revision: 8, mask: MASK.WAITING },
      "a legacy bare Codex selection resumes even when its groupId starts with the OpenCode prefix");
    pass("legacy persisted bare Codex selections load and resume as Codex");
  } finally {
    await broker.close();
  }
}

async function testBrokerRecoveryAndFailures() {
  const root = await mkdtemp("/tmp/pomodoro-recovery-");
  const stateDir = path.join(root, "pomodoro", "agent-status");
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

async function testOnDemandBroker() {
  const root = await mkdtemp("/tmp/pomodoro-on-demand-");
  const env = { ...process.env, XDG_STATE_HOME: root, POMODORO_PORT: "/missing" };
  const socketPath = brokerSocketPath(env);
  const sessionA = `demand-a-${process.pid}-${Date.now()}`;
  const sessionB = `demand-b-${process.pid}-${Date.now()}`;
  let reporterA;
  let reporterB;
  try {
    reporterA = await startReporter({ sessionId: sessionA, cwd: process.cwd(), hostPid: process.pid, env, intervalMs: 80 });
    reporterB = await startReporter({ sessionId: sessionB, cwd: process.cwd(), hostPid: process.pid, env, intervalMs: 80 });
    const [selectedA, selectedB] = await Promise.all([
      request(reporterA.localSocket, hook("UserPromptSubmit", { session_id: sessionA, prompt: "$pomodoro-select" }), 1_500),
      request(reporterB.localSocket, hook("UserPromptSubmit", { session_id: sessionB, prompt: "$pomodoro-select" }), 1_500),
    ]);
    assert.equal(selectedA.ok && selectedB.ok, true);
    assert.equal(await ensureBroker(socketPath, { env }), false, "an existing broker is reused");
    pass("selection starts one on-demand broker and reuses its socket under concurrency");
  } finally {
    await reporterA?.close();
    await reporterB?.close();
  }
  await waitFor(() => stat(socketPath).then(() => false, () => true), MEMBER_LEASE_MS + 2_000);
  pass("on-demand broker exits after reports stop");
}

await testPureCore();
await testHookSelection();
await testReporterIntegration();
await testOpenCodeActivation();
await testUnselectedExpiryIsolation();
await testLegacyPersistedSelection();
await testBrokerRecoveryAndFailures();
await testOnDemandBroker();
console.log(`pomodoro-agent-status check: ${checks} checks passed`);
