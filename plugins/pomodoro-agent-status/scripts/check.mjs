#!/usr/bin/env node

import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { chmod, mkdtemp, mkdir, readFile, readdir, realpath, stat, unlink, writeFile } from "node:fs/promises";
import net from "node:net";
import os from "node:os";
import path from "node:path";
import process from "node:process";
import { fileURLToPath, pathToFileURL } from "node:url";

const scriptsDir = path.dirname(fileURLToPath(import.meta.url));
import { BrokerCore, MEMBER_LEASE_MS, runBroker } from "./broker.mjs";
import { ActivationSession, MemberState, eventSessionId, memberId, normalizeEventType, reduceMemberEvent } from "./opencode-core.mjs";
import {
  MASK, StatusState, brokerSocketPath, discoverHostPid, ensureBroker, hashId, isSelectionPrompt, reporterSocketPath,
  request, startReporter, structuredFailure, validateHookInput, workspaceRoot,
} from "./reporter.mjs";

let checks = 0;
function pass(name) { console.log(`ok ${++checks}: ${name}`); }
function hook(event, fields = {}) {
  return { session_id: "session-a", cwd: process.cwd(), hook_event_name: event, ...fields };
}
async function waitFor(test, timeout = 2_000, message = "timed out waiting for condition") {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) {
    if (await test()) return;
    await new Promise((resolve) => setTimeout(resolve, 20));
  }
  throw new Error(message);
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

  const member = new MemberState();
  const opencodeEvent = (type, properties) => ({ type, properties });
  reduceMemberEvent(member, opencodeEvent("permission.updated", { sessionID: "s", id: "p1" }));
  assert.equal(member.mask, MASK.WAITING, "the installed 1.18.29 permission.updated event maps to Waiting");
  reduceMemberEvent(member, opencodeEvent("permission.replied", { sessionID: "s", permissionID: "p1", response: "once" }));
  assert.equal(member.mask, MASK.WORKING, "the installed permission.replied event resumes Working");
  reduceMemberEvent(member, opencodeEvent("permission.v2.asked", { sessionID: "s" }));
  assert.equal(member.mask, MASK.WAITING, "namespaced permission aliases map to Waiting");
  reduceMemberEvent(member, opencodeEvent("question.v2.rejected", { sessionID: "s", requestID: "q1" }));
  assert.equal(member.mask, MASK.WORKING, "namespaced question aliases resume Working");
  reduceMemberEvent(member, opencodeEvent("message.part.updated", {
    part: { type: "tool", tool: "question", sessionID: "s", callID: "c", state: { status: "running", input: {} } },
  }));
  assert.equal(member.mask, MASK.WAITING, "a running Question tool part maps to Waiting without a question event");
  reduceMemberEvent(member, opencodeEvent("message.part.updated", {
    part: { type: "tool", tool: "question", sessionID: "s", callID: "c", state: { status: "error", input: {}, error: "rejected", time: { start: 1, end: 2 } } },
  }));
  assert.equal(member.mask, MASK.WORKING, "an ended Question tool interaction resumes Working without latching Error");
  reduceMemberEvent(member, opencodeEvent("message.part.updated", {
    part: { type: "tool", tool: "bash", sessionID: "s", callID: "c2", state: { status: "completed", input: {}, output: "", title: "", metadata: {}, time: { start: 1, end: 2 } } },
  }));
  assert.equal(member.mask, MASK.WORKING, "a completed ordinary tool part keeps Working");
  assert.equal(eventSessionId(opencodeEvent("message.updated", { info: { id: "msg-1", sessionID: "ses-9", role: "user" } })), "ses-9",
    "message attribution uses info.sessionID, never the message id");
  assert.equal(eventSessionId(opencodeEvent("session.updated", { info: { id: "ses-9", directory: "/x" } })), "ses-9");
  assert.equal(eventSessionId(opencodeEvent("message.part.updated", { part: { sessionID: "ses-9" } })), "ses-9");
  pass("OpenCode event compatibility aliases, Question tool detection, and shape-aware attribution");
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
    const firstReply = await brokerRequest(broker.socketPath, opencodeReport);
    assert.deepEqual(firstReply, { ok: true, selected: null, revision: 0 },
      "responses keep the historical shape while nothing is selected");
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
    assert.deepEqual(codexSelected, { ok: true, selected: groupId, revision: 1 },
      "Codex responses keep the historical bare selected value and field set");
    assert.deepEqual(broker.core.publication(), { revision: 1, mask: MASK.WAITING },
      "Codex first-valid-source still selects the same groupId");

    await brokerRequest(broker.socketPath, { ...opencodeReport, mask: MASK.ERROR });
    assert.equal(broker.core.selected, `codex:${groupId}`,
      "the Codex source and the OpenCode source with the same groupId stay distinct");
    assert.deepEqual(broker.core.publication(), { revision: 1, mask: MASK.WAITING },
      "unselected OpenCode reports cannot change the selected revision or mask");

    const activated = await brokerRequest(broker.socketPath, { type: "select", groupId, host: "opencode" });
    assert.deepEqual(activated, { ok: true, selected: groupId, revision: 2, selectedHost: "opencode" },
      "activation is unambiguous: bare groupId plus an explicit OpenCode discriminator");
    assert.equal(activated.revision, 2, "first activation creates exactly one new revision");
    assert.deepEqual(broker.core.publication(), { revision: 2, mask: MASK.ERROR | MASK.WORKING },
      "the selected OpenCode aggregate excludes the Codex source with the same groupId");

    const reactivated = await brokerRequest(broker.socketPath, { type: "select", groupId, host: "opencode" });
    assert.deepEqual(reactivated, { ok: true, selected: groupId, revision: 3, selectedHost: "opencode" },
      "reactivation creates exactly one new revision");

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
    assert.deepEqual(codexExplicit, { ok: true, selected: groupId, revision: 7 },
      "explicit Codex selection excludes the OpenCode aggregate");
    assert.deepEqual(broker.core.publication(), { revision: 7, mask: MASK.WAITING },
      "explicit Codex selection excludes the OpenCode aggregate");

    const oddGroupId = `opencode:${groupId}`;
    const oddReport = { type: "report", groupId: oddGroupId, memberId: "codex-odd", pid: process.pid, startedAt, mask: MASK.WORKING };
    const oddReported = await brokerRequest(broker.socketPath, oddReport);
    assert.equal(oddReported.ok, true, "previously valid Codex groupIds remain valid");
    assert.deepEqual(broker.core.publication(), { revision: 7, mask: MASK.WAITING },
      "the odd Codex source is unselected and cannot change publication");
    const oddSelected = await brokerRequest(broker.socketPath, { type: "select", groupId: oddGroupId });
    assert.deepEqual(oddSelected, { ok: true, selected: oddGroupId, revision: 8 },
      "a Codex selection response carries no host discriminator");
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

function forbiddenClientProxy(counter) {
  return new Proxy({}, {
    get(target, property) {
      counter.clientTouches.push(String(property));
      return () => { throw new Error("model/session API use is forbidden in this check"); };
    },
  });
}

function fakeTuiApi({ directory, worktree, counter }) {
  const disposers = [];
  const commands = [];
  return {
    api: {
      app: { version: "1.18.29" },
      command: {
        register: (factory) => {
          commands.push(factory);
          return () => {};
        },
        trigger: () => counter.triggers++,
        show: () => counter.triggers++,
      },
      ui: { toast: (input) => counter.toasts.push(input) },
      state: { path: { directory, worktree, state: directory, config: directory } },
      client: forbiddenClientProxy(counter),
      lifecycle: {
        signal: new AbortController().signal,
        onDispose(fn) { disposers.push(fn); return () => {}; },
      },
    },
    commands: () => commands.flatMap((factory) => factory()),
    toasts: () => counter.toasts,
    dispose: () => Promise.all(disposers.map((fn) => fn())),
  };
}

async function testOpenCodePluginSelection() {
  const root = await mkdtemp("/tmp/pomodoro-opencode-plugin-");
  const workspace = path.join(root, "workspace");
  await mkdir(workspace);
  const stateDir = path.join(root, "pomodoro", "agent-status");
  const env = { ...process.env, XDG_STATE_HOME: root };
  const counter = { toasts: [], triggers: 0, clientTouches: [] };
  const tui = fakeTuiApi({ directory: workspace, worktree: workspace, counter });
  const socketGone = () => stat(broker.socketPath).then(() => false, () => true);
  const broker = await runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
  const serverInput = {
    project: { id: "proj" },
    directory: workspace,
    worktree: workspace,
    client: forbiddenClientProxy(counter),
    serverUrl: new URL("http://127.0.0.1:0"),
    $: undefined,
  };
  try {
    const serverModule = await import("./opencode-server.mjs");
    const serverExport = Object.entries(serverModule).filter(([, value]) => typeof value === "function");
    assert.equal(serverExport.length, 1, "the server plugin module exports exactly one plugin function");
    const hooks = await serverExport[0][1](serverInput, { env });

    const tuiModule = await import("./opencode-tui.mjs");
    assert.equal(typeof tuiModule.default, "object", "the TUI module is a { id, tui } module");
    await tuiModule.default.tui(tui.api, { env });
    const commands = tui.commands();
    assert.equal(commands.length, 1, "exactly one TUI command is registered");
    assert.deepEqual(commands[0].slash, { name: "pomodoro-select" });
    assert.equal(typeof commands[0].onSelect, "function");

    const groupId = hashId(await realpath(workspace));
    const before = broker.core.revision;
    commands[0].onSelect();
    const selection = await waitFor(() => broker.core.selected === `opencode:${groupId}` && broker.core.revision === before + 1)
      .then(() => true, () => { throw new Error("selection did not create exactly one revision"); });
    assert.equal(selection, true);
    assert.deepEqual(broker.core.publication(), { revision: before + 1, mask: MASK.IDLE });
    assert.equal(counter.triggers, 0, "selection did not trigger any command dispatch");
    await waitFor(() => tui.toasts().length === 1);
    assert.match(tui.toasts()[0].message, /revision/);
    assert.deepEqual(counter.clientTouches, [], "neither plugin touched the SDK client (no model, message, or Task use)");

    commands[0].onSelect();
    await waitFor(() => broker.core.revision === before + 2, 2_000);
    assert.equal(broker.core.revision, before + 2, "repeat selection creates exactly one more revision");
    assert.equal(counter.clientTouches.length, 0);
    await waitFor(() => tui.toasts().length === 2);
    assert.match(tui.toasts()[1].message, /revision/);

    const tuiSession = `ses-tui-${process.pid}`;
    await hooks.event({ event: { type: "session.created", properties: { info: { id: tuiSession, directory: workspace } } } });
    await hooks.event({ event: { type: "session.status", properties: { sessionID: tuiSession, status: { type: "busy" } } } });
    await waitFor(() => broker.core.publication()?.mask === MASK.WORKING);
    assert.equal(broker.core.selected, `opencode:${groupId}`);
    await hooks.event({ event: { type: "session.idle", properties: { sessionID: tuiSession } } });
    await waitFor(() => broker.core.publication()?.mask === MASK.COMPLETED);
    assert.equal(counter.clientTouches.length, 0, "the lifecycle path created no client or model effects");
    await hooks.dispose();

    const persistedBefore = await readFile(path.join(stateDir, "agent-coordination.json"), "utf8");
    await broker.close();
    const impostorSockets = new Set();
    const impostor = net.createServer((socket) => {
      impostorSockets.add(socket);
      socket.once("close", () => impostorSockets.delete(socket));
      socket.end("not-json\n");
    });
    await new Promise((resolve) => impostor.listen(broker.socketPath, resolve));
    tui.commands()[0].onSelect();
    await waitFor(() => tui.toasts().length === 3);
    assert.equal(tui.toasts()[2].variant, "error", "selection failure feedback is bounded and visible");
    assert.doesNotMatch(tui.toasts()[2].message, /Error:|at /, "failure feedback exposes no error details");
    assert.deepEqual(counter.clientTouches, []);
    for (const socket of impostorSockets) socket.destroy();
    await new Promise((resolve) => impostor.close(() => resolve()));
    assert.equal(await readFile(path.join(stateDir, "agent-coordination.json"), "utf8"), persistedBefore,
      "the failed selection neither changed nor overwrote the persisted selection");

    tui.commands()[0].onSelect();
    await waitFor(() => tui.toasts().length === 4);
    assert.equal(tui.toasts()[3].variant, "info", "a freshly started broker reports waiting-for-takeover feedback");
    assert.match(tui.toasts()[3].message, /waiting for the broker/);
    const resumed = await request(brokerSocketPath(env), {
      type: "report", groupId: hashId(await realpath(workspace)), memberId: "check-probe",
      pid: process.pid, startedAt: Date.now(), mask: MASK.IDLE, host: "opencode",
    }, 1_000);
    const persistedRevision = JSON.parse(persistedBefore).revision;
    assert.deepEqual(resumed, { ok: true, selected: hashId(await realpath(workspace)), revision: persistedRevision + 1, selectedHost: "opencode" },
      "the on-demand broker restored the persisted selection with exactly one new revision");
    pass("real plugin entry points select the Workspace with one revision, publish Working/Completed, start the broker on demand, and fail without a broker");
  } finally {
    await tui.dispose().catch(() => {});
    await broker.close().catch(() => {});
    await waitFor(socketGone, MEMBER_LEASE_MS + 4_000);
  }
}

function fakeServerInput({ directory, worktree, counter }) {
  return {
    project: { id: "proj" },
    directory,
    worktree,
    client: forbiddenClientProxy(counter),
    serverUrl: new URL("http://127.0.0.1:0"),
    $: undefined,
  };
}

async function loadServerPlugin(input, options) {
  const serverModule = await import("./opencode-server.mjs");
  const exports = Object.entries(serverModule).filter(([, value]) => typeof value === "function");
  assert.equal(exports.length, 1, "the server plugin module exports exactly one plugin function");
  const hooks = await exports[0][1](input, options);
  return hooks;
}

async function spawnOpenCodeProcess({ socketPath, groupId, mask, intervalMs = 150 }) {
  const script = `
import { request } from ${JSON.stringify(pathToFileURL(path.join(scriptsDir, "opencode-core.mjs")).href)};
const socketPath = process.env.CHILD_SOCKET;
const groupId = process.env.CHILD_GROUP;
const mask = Number(process.env.CHILD_MASK);
const startedAt = Date.now();
const keep = setInterval(() => {
  request(socketPath, { type: "report", groupId, memberId: "child-member", pid: process.pid, startedAt, mask, host: "opencode" }).catch(() => {});
}, ${intervalMs});
process.on("disconnect", () => { clearInterval(keep); process.exit(0); });
`;
  return spawn(process.execPath, ["--input-type=module", "-e", script], {
    env: { ...process.env, CHILD_SOCKET: socketPath, CHILD_GROUP: groupId, CHILD_MASK: String(mask) },
    stdio: ["ignore", "ignore", "inherit"],
  });
}

async function testOpenCodeLifecycle() {
  const root = await mkdtemp("/tmp/pomodoro-opencode-lifecycle-");
  const workspace = path.join(root, "workspace");
  await mkdir(workspace);
  const stateDir = path.join(root, "pomodoro", "agent-status");
  const env = { ...process.env, XDG_STATE_HOME: root };
  const counter = { toasts: [], triggers: 0, clientTouches: [] };
  const broker = await runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
  const groupId = hashId(await realpath(workspace));
  const hooks = await loadServerPlugin(fakeServerInput({ directory: workspace, worktree: workspace, counter }), { env });
  const session = (id, extra = {}) => ({ info: { id, directory: workspace, ...extra } });
  let child = null;
  try {
    await hooks.event({ event: { type: "session.created", properties: session("s1") } });
    await hooks.event({ event: { type: "session.created", properties: session("s2") } });
    await hooks.event({ event: { type: "session.created", properties: session("s3", { parentID: "s1" }) } });
    await waitFor(() => broker.core.sourceIsLive(`opencode:${groupId}`) &&
      broker.core.members.get(`opencode:${groupId}`)?.size === 3, 2_000);
    const selected = await brokerRequest(broker.socketPath, { type: "select", groupId, host: "opencode" });
    assert.equal(selected.ok, true, "explicit selection requires the live members reported by the plugin");
    const revisionAtSelection = selected.revision;
    const revBefore = broker.core.revision;

    await hooks.event({ event: { type: "session.status", properties: { sessionID: "s1", status: { type: "busy" } } } });
    await hooks.event({ event: { type: "session.status", properties: { sessionID: "s3", status: { type: "retry", attempt: 1, message: "x", next: 1 } } } });
    await hooks.event({ event: {
      type: "permission.updated",
      properties: { id: "p1", type: "bash", sessionID: "s2", messageID: "m", title: "bash", metadata: {}, time: { created: 1 } },
    } });
    await waitFor(() => broker.core.publication()?.mask === (MASK.WORKING | MASK.WAITING));
    assert.deepEqual(broker.core.publication(), { revision: revBefore + 2, mask: MASK.WORKING | MASK.WAITING },
      "busy, retry, and the installed permission.updated event aggregate; each aggregate change advances exactly once");

    await hooks.event({ event: {
      type: "message.part.updated",
      properties: { part: { id: "qp", sessionID: "s2", messageID: "m", type: "tool", callID: "cq", tool: "question", state: { status: "running", input: {} } } },
    } });
    await hooks.event({
      event: { type: "message.part.updated", properties: { part: { id: "p", sessionID: "s1", messageID: "m", type: "tool", callID: "c", tool: "bash", state: { status: "error", input: {}, error: "boom", time: { start: 1, end: 2 } } } } },
    });
    await waitFor(() => broker.core.publication()?.mask === (MASK.ERROR | MASK.WAITING | MASK.WORKING));
    await hooks.event({ event: { type: "session.idle", properties: { sessionID: "s2" } } });
    await waitFor(() => broker.core.publication()?.mask === (MASK.ERROR | MASK.COMPLETED | MASK.WORKING),
      2_000, "Question and tool-error members keep their states while another member completes");

    const memberCountBeforeUserMessage = broker.core.members.get(`opencode:${groupId}`).size;
    await hooks.event({ event: {
      type: "message.updated",
      properties: { info: { id: "msg-1", sessionID: "s1", role: "user", agent: "build", model: { providerID: "p", modelID: "m" } } },
    } });
    await waitFor(() => broker.core.publication()?.mask === (MASK.WORKING | MASK.COMPLETED | MASK.WORKING),
      2_000, "real new work clears only that member's latched Error");
    assert.equal(broker.core.members.get(`opencode:${groupId}`).size, memberCountBeforeUserMessage,
      "a message-shaped event attributes to the Session and never creates a phantom member");
    assert.equal(broker.core.members.get(`opencode:${groupId}`).has(memberId("msg-1")), false,
      "the message id is not used as a member identity");
    assert.equal([...broker.core.members.get(`opencode:${groupId}`).values()].some((m) => m.mask === MASK.ERROR), false,
      "the latched Error is cleared for the working member");

    await hooks.event({ event: { type: "message.part.updated", properties: { part: { id: "p2", sessionID: "s1", messageID: "m", type: "tool", callID: "c2", tool: "bash", state: { status: "error", input: {}, error: "boom", time: { start: 1, end: 2 } } } } } });
    await waitFor(() => broker.core.publication()?.mask === (MASK.ERROR | MASK.COMPLETED | MASK.WORKING),
      2_000, "a latched Error survives other members' idle and completion");
    await hooks.event({ event: { type: "permission.replied", properties: { sessionID: "s2", permissionID: "p1", response: "once" } } });
    await hooks.event({ event: {
      type: "message.part.updated",
      properties: { part: { id: "qp", sessionID: "s2", messageID: "m", type: "tool", callID: "cq", tool: "question", state: { status: "completed", input: {}, output: "", title: "", metadata: {}, time: { start: 1, end: 2 } } } },
    } });
    await waitFor(() => broker.core.publication()?.mask === (MASK.ERROR | MASK.WORKING | MASK.WORKING));

    await hooks.event({ event: { type: "session.deleted", properties: session("s3") } });
    await waitFor(() => broker.core.members.get(`opencode:${groupId}`)?.size === 2, 2_000, "session deletion removes only that member");
    assert.deepEqual(broker.core.publication()?.mask, MASK.ERROR | MASK.WORKING);

    const workspace2 = path.join(root, "workspace-2");
    await mkdir(workspace2);
    const group2 = hashId(await realpath(workspace2));
    await hooks.event({ event: { type: "session.created", properties: session("s4", { directory: workspace2 }) } });
    await hooks.event({ event: { type: "session.status", properties: { sessionID: "s4", status: { type: "busy" } } } });
    await waitFor(() => [...(broker.core.members.get(`opencode:${group2}`) ?? new Map()).values()].some((m) => m.mask === MASK.WORKING), 2_000,
      "a session in another directory joins that Workspace");
    await hooks.event({ event: { type: "session.updated", properties: session("s4", { directory: workspace }) } });
    await waitFor(() => (broker.core.members.get(`opencode:${group2}`)?.size ?? 0) === 0, 2_000,
      "session.updated moves the member out of the old Workspace");
    await hooks.event({ event: {
      type: "permission.updated",
      properties: { id: "p4", type: "bash", sessionID: "s4", messageID: "m", title: "bash", metadata: {}, time: { created: 1 } },
    } });
    await waitFor(() => [...broker.core.members.get(`opencode:${groupId}`).values()].some((m) => m.mask === MASK.WAITING), 2_000,
      "after the move, lifecycle reports address only the new Workspace");
    assert.equal(broker.core.members.get(`opencode:${group2}`)?.size ?? 0, 0,
      "the stale Workspace received no further reports");
    await hooks.event({ event: { type: "session.deleted", properties: session("s4", { directory: workspace }) } });
    await waitFor(() => broker.core.members.get(`opencode:${groupId}`)?.size === 2, 2_000);

    child = await spawnOpenCodeProcess({ socketPath: broker.socketPath, groupId, mask: MASK.WORKING });
    await waitFor(() => broker.core.members.get(`opencode:${groupId}`)?.size === 3, 3_000,
      "a member from a separate OpenCode process aggregates into the same Workspace");
    assert.equal(broker.core.selected, `opencode:${groupId}`);
    child.kill("SIGKILL");
    child = null;
    await waitFor(() => broker.core.members.get(`opencode:${groupId}`)?.size === 2, MEMBER_LEASE_MS + 3_000,
      "a dead process member is removed by lease/PID cleanup");
    assert.equal(broker.core.publication()?.mask, MASK.ERROR | MASK.WORKING,
      "removing the dead member did not disturb the live aggregate");

    await hooks.dispose();
    await waitFor(() => broker.core.publication() === null, MEMBER_LEASE_MS + 3_000,
      "plugin disposal stops lease renewal, so all members expire and publication stops");
    assert.equal(broker.core.selected, `opencode:${groupId}`, "the Workspace stays selected while all members are gone");
    const codexWhileVacant = await brokerRequest(broker.socketPath, { type: "report", groupId, memberId: "codex-lurker", pid: process.pid, startedAt: Date.now(), mask: MASK.WAITING });
    assert.equal(codexWhileVacant.ok, true);
    assert.equal(broker.core.selected, `opencode:${groupId}`, "a Codex report cannot take over the vacant selected Workspace");
    assert.equal(broker.core.publication(), null);

    const returning = await loadServerPlugin(fakeServerInput({ directory: workspace, worktree: workspace, counter }), { env });
    await returning.event({ event: { type: "session.created", properties: session("s1") } });
    await returning.event({ event: { type: "session.status", properties: { sessionID: "s1", status: { type: "busy" } } } });
    await returning.event({ event: { type: "session.status", properties: { sessionID: "s-resumed", status: { type: "busy" } } } });
    await waitFor(() => broker.core.publication()?.mask === MASK.WORKING, MEMBER_LEASE_MS + 3_000,
      "a returning member of the same Workspace restores publication");
    await waitFor(() => broker.core.members.get(`opencode:${groupId}`)?.size === 2, 2_000,
      "a session resumed after an OpenCode restart (no session.created) joins as a member of the process Workspace");
    assert.equal(broker.core.selected, `opencode:${groupId}`);
    assert.ok(broker.core.revision > revisionAtSelection, "restoring publication allocated a new revision");
    assert.deepEqual(counter.clientTouches, [], "the complete lifecycle created no client or model effects");
    await returning.dispose();
    pass("complete OpenCode lifecycle aggregates members across processes with latched errors, removal, vacancy, and recovery");
  } finally {
    if (child) child.kill("SIGKILL");
    await hooks.dispose().catch(() => {});
    await broker.close().catch(() => {});
  }
}

async function testCrossHostPluginIsolation() {
  const root = await mkdtemp("/tmp/pomodoro-cross-host-");
  const workspace = await realpath(process.cwd());
  const stateDir = path.join(root, "pomodoro", "agent-status");
  const env = { ...process.env, XDG_STATE_HOME: root };
  const counter = { toasts: [], triggers: 0, clientTouches: [] };
  const groupId = hashId(workspace);
  const serialLines = [];
  let opens = 0;
  let transport = null;
  const broker = await runBroker({
    stateDir,
    logger: () => {},
    publisherOptions: {
      resolvePort: async () => "/dev/fake-cu",
      connect: async (port, onLine) => {
        opens++;
        transport = {
          write: async (line) => { serialLines.push(line); },
          close: async () => { transport = null; },
        };
        setImmediate(() => onLine("OC1 ENABLED"));
        return transport;
      },
    },
  });
  const codexSession = `codex-host-${process.pid}`;
  const reporter = await startReporter({ sessionId: codexSession, cwd: workspace, hostPid: process.pid, env, intervalMs: 60 });
  const hooks = await loadServerPlugin(fakeServerInput({ directory: workspace, worktree: workspace, counter }), { env });
  const codexEvent = (event, fields = {}) => hook(event, { session_id: codexSession, cwd: workspace, ...fields });
  try {
    await request(reporter.localSocket, codexEvent("UserPromptSubmit", { prompt: "codex work" }));
    await request(reporter.localSocket, codexEvent("PermissionRequest", { tool_name: "Bash" }));
    await waitFor(() => broker.core.publication()?.mask === MASK.WAITING, 3_000,
      "the first valid Codex report selects Codex and publishes its aggregate");
    const codexRevision = broker.core.revision;
    await waitFor(() => serialLines.includes(`OC1 STATE ${codexRevision} ${MASK.WAITING}`), 4_000,
      "the broker is the only serial owner and publishes over OC1 while both hosts report");

    await hooks.event({ event: { type: "session.created", properties: { info: { id: "oc-solo", directory: workspace } } } });
    await hooks.event({ event: { type: "session.status", properties: { sessionID: "oc-solo", status: { type: "busy" } } } });
    await waitFor(() => broker.core.members.get(`opencode:${groupId}`)?.size === 1);
    assert.deepEqual(broker.core.publication(), { revision: codexRevision, mask: MASK.WAITING },
      "OpenCode Working while Codex is selected does not alter the selected revision or mask");

    const activation = await new ActivationSession({ root: workspace, env }).activate();
    assert.equal(activation.outcome, "selected");
    assert.deepEqual(broker.core.publication(), { revision: codexRevision + 1, mask: MASK.WORKING },
      "with Codex Waiting and OpenCode Working in the same repository, selecting OpenCode publishes only Working");
    await request(reporter.localSocket, codexEvent("PostToolUse", { tool_name: "Bash", tool_response: { exit_code: 2 } }));
    await request(reporter.localSocket, codexEvent("Stop"));
    await waitFor(() => [...broker.core.members.get(`codex:${groupId}`)?.values() ?? []].some((m) => m.mask === MASK.ERROR), 3_000,
      "the unselected Codex member latched an Error report");
    assert.deepEqual(broker.core.publication(), { revision: codexRevision + 1, mask: MASK.WORKING },
      "Error from the unselected host stays isolated from device publication");
    await request(reporter.localSocket, codexEvent("UserPromptSubmit", { prompt: "recovery work" }));
    await request(reporter.localSocket, codexEvent("PermissionRequest", { tool_name: "Bash" }));
    await waitFor(() => [...broker.core.members.get(`codex:${groupId}`).values()].every((m) => m.mask === MASK.WAITING), 3_000,
      "the unselected Codex member recovered to Waiting");
    assert.deepEqual(broker.core.publication(), { revision: codexRevision + 1, mask: MASK.WORKING },
      "recovery reports from the unselected host stay isolated from device publication");

    const codexBack = await request(reporter.localSocket, codexEvent("UserPromptSubmit", { prompt: "$pomodoro-select" }));
    assert.equal(codexBack.ok, true);
    const codexRevised = codexBack.revision;
    assert.deepEqual(broker.core.publication(), { revision: codexRevised, mask: MASK.WAITING },
      "selecting Codex afterward publishes only Waiting");
    await hooks.event({ event: { type: "session.error", properties: { sessionID: "oc-solo", error: { name: "ApiError" } } } });
    await hooks.event({ event: { type: "session.status", properties: { sessionID: "oc-solo", status: { type: "retry", attempt: 1, message: "x", next: 1 } } } });
    await waitFor(() => [...broker.core.members.get(`opencode:${groupId}`).values()].some((m) => m.mask === MASK.ERROR), 3_000,
      "the unselected OpenCode member latched an Error");
    assert.deepEqual(broker.core.publication(), { revision: codexRevised, mask: MASK.WAITING },
      "later Error and recovery reports from the unselected OpenCode host cannot alter the selected revision or mask");

    const [firstAttempt, secondAttempt] = await Promise.all([
      brokerRequest(broker.socketPath, { type: "select", groupId, host: "opencode" }),
      brokerRequest(broker.socketPath, { type: "select", groupId }),
    ]);
    assert.equal(firstAttempt.ok && secondAttempt.ok, true);
    assert.equal(Math.abs(firstAttempt.revision - secondAttempt.revision), 1,
      "concurrent cross-host selections stay serialized");
    assert.ok(broker.core.revision >= Math.max(firstAttempt.revision, secondAttempt.revision),
      "revisions stay monotonic across concurrent selections");

    await writeFile(path.join(stateDir, "flash-reservation"), `${process.pid}\n`, { mode: 0o600 });
    const opensBeforeReservation = opens;
    await waitFor(() => transport === null, 2_000, "flash reservation makes the broker release the serial port");
    assert.equal(opens, opensBeforeReservation, "the broker does not reopen the port while the reservation is held");
    await request(reporter.localSocket, codexEvent("UserPromptSubmit", { prompt: "work during flashing" }));
    await hooks.event({ event: { type: "session.status", properties: { sessionID: "oc-solo", status: { type: "busy" } } } });
    await waitFor(() => broker.core.publication() !== null, 2_000, "IPC and publication keep working during the reservation");
    await unlink(path.join(stateDir, "flash-reservation"));
    await waitFor(() => transport !== null && serialLines.some((line) => line.startsWith("OC1 STATE")), 3_000,
      "communication resumes after the reservation is removed");
    pass("cross-host isolation through real plugin surfaces with single serial ownership and flash reservation release");
  } finally {
    if (transport) await transport.close().catch(() => {});
    await reporter.close().catch(() => {});
    await hooks.dispose().catch(() => {});
    await broker.close().catch(() => {});
  }
}

async function testInstallationAndRecovery() {
  const root = await mkdtemp("/tmp/pomodoro-install-");
  const configDir = path.join(root, "opencode-config");
  await mkdir(configDir, { recursive: true });
  const installer = await import("./install-opencode.mjs");
  const installOptions = { configDir, opencodeVersion: "1.18.29" };

  await installer.install(installOptions);
  await installer.install(installOptions);
  const pluginDir = path.join(configDir, "plugins", "pomodoro-agent-status");
  const installedFiles = (await readdir(pluginDir)).sort();
  assert.deepEqual(installedFiles, ["broker.mjs", "opencode-core.mjs", "opencode-server.mjs", "opencode-tui.mjs", "reporter.mjs"],
    "the user-level plugin directory holds exactly the authoritative surfaces");
  for (const file of installedFiles) {
    assert.equal(await readFile(path.join(pluginDir, file), "utf8"), await readFile(path.join(scriptsDir, file), "utf8"),
      `installed ${file} is byte-identical to the repository source`);
  }
  const serverConfig = JSON.parse(await readFile(path.join(configDir, "opencode.json"), "utf8"));
  const tuiConfig = JSON.parse(await readFile(path.join(configDir, "tui.json"), "utf8"));
  assert.equal(serverConfig.plugin.length, 1, "the server plugin is registered exactly once");
  assert.equal(tuiConfig.plugin.length, 1, "the TUI plugin is registered exactly once");
  assert.equal(serverConfig.plugin[0], path.join(pluginDir, "opencode-server.mjs"));
  assert.equal(tuiConfig.plugin[0], path.join(pluginDir, "opencode-tui.mjs"));

  const projectA = path.join(root, "project-a");
  const projectB = path.join(root, "project-b");
  const pomodoroProject = await realpath(path.join(scriptsDir, "..", "..", ".."));
  const projectCopies = async (projectDir) => {
    try {
      return (await readdir(path.join(projectDir, ".opencode", "plugins"))).filter((name) => name.toLowerCase().includes("pomodoro")).length;
    } catch (error) {
      if (error?.code === "ENOENT") return 0;
      throw error;
    }
  };
  await mkdir(path.join(projectA, ".opencode"), { recursive: true });
  for (const projectDir of [projectA, projectB, pomodoroProject]) {
    await installer.verify({ ...installOptions, projectDir });
    assert.equal(await projectCopies(projectDir), 0, `no project-level Pomodoro plugin copy exists for ${projectDir}`);
  }
  const projectSingular = path.join(root, "project-singular");
  const projectPlural = path.join(root, "project-plural");
  await mkdir(path.join(projectSingular, ".opencode", "plugin", "pomodoro-copy"), { recursive: true });
  await mkdir(path.join(projectPlural, ".opencode", "plugins", "pomodoro-copy"), { recursive: true });
  await assert.rejects(installer.verify({ ...installOptions, projectDir: projectSingular }), /project-level Pomodoro plugin copy/,
    "a singular .opencode/plugin duplicate is rejected");
  await assert.rejects(installer.verify({ ...installOptions, projectDir: projectPlural }), /project-level Pomodoro plugin copy/,
    "a plural .opencode/plugins duplicate is rejected");

  const serverConfigPath = path.join(configDir, "opencode.json");
  const tuiConfigPath = path.join(configDir, "tui.json");
  const relativeServer = "plugins/pomodoro-agent-status/opencode-server.mjs";
  const relativeTui = "plugins/pomodoro-agent-status/opencode-tui.mjs";
  const rewrite = async (configPath, relative) => {
    const config = JSON.parse(await readFile(configPath, "utf8"));
    config.plugin = [relative];
    await writeFile(configPath, `${JSON.stringify(config, null, 2)}\n`, { mode: 0o600 });
  };
  await rewrite(serverConfigPath, relativeServer);
  await rewrite(tuiConfigPath, relativeTui);
  await installer.install(installOptions);
  assert.deepEqual(JSON.parse(await readFile(serverConfigPath, "utf8")).plugin, [relativeServer],
    "a genuine relative entry is recognized and never duplicated");
  assert.deepEqual(JSON.parse(await readFile(tuiConfigPath, "utf8")).plugin, [relativeTui]);
  await installer.verify(installOptions);
  await installer.uninstall({ configDir });
  assert.equal(JSON.parse(await readFile(serverConfigPath, "utf8")).plugin, undefined, "uninstall matches the relative server entry");
  assert.equal(JSON.parse(await readFile(tuiConfigPath, "utf8")).plugin, undefined, "uninstall matches the relative TUI entry");
  await installer.install(installOptions);
  pass("idempotent user-level installation serves multiple projects without a project-level duplicate");

  const badShapeConfig = path.join(root, "bad-shape-config");
  await mkdir(badShapeConfig, { recursive: true });
  await installer.install({ configDir: badShapeConfig, opencodeVersion: "1.18.29" });
  const serverBefore = await readFile(path.join(badShapeConfig, "opencode.json"), "utf8");
  const brokenTui = JSON.stringify({ plugin: "not-an-array" }, null, 2);
  await writeFile(path.join(badShapeConfig, "tui.json"), `${brokenTui}\n`, { mode: 0o600 });
  await assert.rejects(installer.uninstall({ configDir: badShapeConfig }), /not an array/,
    "uninstall refuses a non-array tui plugin before mutating opencode.json");
  assert.equal(await readFile(path.join(badShapeConfig, "opencode.json"), "utf8"), serverBefore,
    "the valid server config is byte-identical after the refused uninstall");
  assert.equal(await readFile(path.join(badShapeConfig, "tui.json"), "utf8"), `${brokenTui}\n`,
    "the invalid TUI config is byte-identical after the refused uninstall");
  assert.deepEqual((await readdir(path.join(badShapeConfig, "plugins", "pomodoro-agent-status"))).sort(),
    ["broker.mjs", "opencode-core.mjs", "opencode-server.mjs", "opencode-tui.mjs", "reporter.mjs"],
    "the installed plugin directory survives a refused uninstall untouched");

  const oldConfigDir = path.join(root, "old-config");
  await mkdir(oldConfigDir, { recursive: true });
  await assert.rejects(installer.install({ configDir: oldConfigDir, opencodeVersion: "1.18.0" }), /1\.18\.29/,
    "installation below the minimum verified version fails explicitly");
  assert.deepEqual(await readdir(oldConfigDir).then((files) => files.filter((name) => name !== "plugins")), [],
    "a failed install leaves no configuration behind");
  const tuiModule = await import("./opencode-tui.mjs");
  await assert.rejects(tuiModule.default.tui({ ui: { toast() {} } }), /api\.command/,
    "TUI initialization fails explicitly without the direct slash API");
  await assert.rejects(tuiModule.default.tui({ command: { register() {}, trigger() {} } }), /toast/);

  const jsoncConfig = path.join(root, "jsonc-config");
  await mkdir(jsoncConfig, { recursive: true });
  const jsonc = `{\n  // user comment that must survive\n  "theme": "opencode"\n}\n`;
  await writeFile(path.join(jsoncConfig, "opencode.jsonc"), jsonc, { mode: 0o600 });
  const tuiPlain = `{\n  "theme": "opencode"\n}\n`;
  await writeFile(path.join(jsoncConfig, "tui.json"), tuiPlain, { mode: 0o600 });
  await assert.rejects(installer.install({ configDir: jsoncConfig, opencodeVersion: "1.18.29" }), /JSONC/,
    "a JSONC-only configuration refuses installation instead of creating a conflicting opencode.json");
  assert.equal(await readFile(path.join(jsoncConfig, "opencode.jsonc"), "utf8"), jsonc,
    "the JSONC source stays byte-identical after refusal");
  assert.equal(await readFile(path.join(jsoncConfig, "tui.json"), "utf8"), tuiPlain,
    "the plain TUI config stays byte-identical after refusal");
  assert.deepEqual(await readdir(jsoncConfig).then((files) => files.sort()), ["opencode.jsonc", "tui.json"],
    "no plugin directory or conflicting config was created");

  await writeFile(path.join(jsoncConfig, "opencode.json"), `${tuiPlain}`, { mode: 0o600 });
  await assert.rejects(installer.install({ configDir: jsoncConfig, opencodeVersion: "1.18.29" }), /JSONC/,
    "a dual JSON/JSONC configuration also refuses");
  await assert.rejects(installer.uninstall({ configDir: jsoncConfig }), /JSONC/,
    "uninstall refuses to edit configurations it cannot parse safely");
  assert.equal(await readFile(path.join(jsoncConfig, "opencode.jsonc"), "utf8"), jsonc);

  const partialConfig = path.join(root, "partial-config");
  await mkdir(partialConfig, { recursive: true });
  const validServer = `{\n  "theme": "x"\n}\n`;
  await writeFile(path.join(partialConfig, "opencode.json"), validServer, { mode: 0o600 });
  await writeFile(path.join(partialConfig, "tui.json"), "{broken\n", { mode: 0o600 });
  await assert.rejects(installer.install({ configDir: partialConfig, opencodeVersion: "1.18.29" }), /not valid JSON/,
    "a malformed TUI config fails before any mutation");
  assert.equal(await readFile(path.join(partialConfig, "opencode.json"), "utf8"), validServer,
    "the valid config is untouched by the failed install");
  assert.deepEqual(await readdir(partialConfig).then((files) => files.sort()), ["opencode.json", "tui.json"],
    "no plugin directory was created for a failed all-or-nothing install");

  const stateRoot = path.join(root, "state");
  const stateDir = path.join(stateRoot, "pomodoro", "agent-status");
  await mkdir(stateDir, { recursive: true, mode: 0o700 });
  const corrupt = "{broken\n";
  await writeFile(path.join(stateDir, "agent-coordination.json"), corrupt, { mode: 0o600 });
  await assert.rejects(runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } }),
    /invalid agent-coordination\.json/, "corrupt persisted state fails closed");
  assert.equal(await readFile(path.join(stateDir, "agent-coordination.json"), "utf8"), corrupt,
    "corrupt persisted state is not overwritten");

  const failOpenStateDir = path.join(root, "absent-state", "pomodoro", "agent-status");
  const failOpenEnv = { ...process.env, XDG_STATE_HOME: path.join(root, "absent-state") };
  const counter = { toasts: [], triggers: 0, clientTouches: [] };
  const hooks = await loadServerPlugin(fakeServerInput({ directory: root, worktree: root, counter }), { env: failOpenEnv });
  await hooks.event({ event: { type: "session.created", properties: { info: { id: "s-fail", directory: root } } } });
  await hooks.event({ event: { type: "session.status", properties: { sessionID: "s-fail", status: { type: "busy" } } } });
  await hooks.event({ event: { type: "message.updated", properties: { sessionID: "s-fail", info: { role: "user" } } } });
  assert.deepEqual(counter.clientTouches, [], "OpenCode lifecycle handling is unaffected while the broker is missing");
  await mkdir(failOpenStateDir, { recursive: true, mode: 0o700 });
  await writeFile(path.join(failOpenStateDir, "agent-coordination.json"), "{broken\n", { mode: 0o600 });
  await assert.rejects(new ActivationSession({ root, env: failOpenEnv }).activate(), /broker/,
    "selection without a working broker fails with a bounded error");
  assert.equal(await readFile(path.join(failOpenStateDir, "agent-coordination.json"), "utf8"), "{broken\n",
    "the failed activation did not overwrite protected state");
  await hooks.dispose();

  const workingStateDir = path.join(path.join(root, "ok-state"), "pomodoro", "agent-status");
  const broker = await runBroker({ stateDir: workingStateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
  const devicelessEnv = { ...process.env, XDG_STATE_HOME: path.join(root, "ok-state") };
  try {
    const activated = await new ActivationSession({ root, env: devicelessEnv }).activate();
    assert.equal(activated.outcome, "selected", "selection succeeds with no device attached");
    assert.deepEqual(broker.core.publication(), { revision: activated.revision, mask: MASK.IDLE },
      "a missing device leaves the broker retrying serial access without failing selection");
    assert.equal((await stat(workingStateDir)).mode & 0o777, 0o700, "the state directory stays restricted");
    assert.equal((await stat(broker.socketPath)).mode & 0o777, 0o600, "the broker socket stays restricted");
    assert.equal((await stat(path.join(workingStateDir, "agent-coordination.json"))).mode & 0o777, 0o600);
    const firstRevision = activated.revision;
    await broker.close();
    const restarted = await runBroker({ stateDir: workingStateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
    try {
      const resumed = await request(restarted.socketPath, {
        type: "report", groupId: activated.groupId, memberId: activated.memberId,
        pid: process.pid, startedAt: Date.now(), mask: MASK.IDLE, host: "opencode",
      }, 1_000);
      assert.equal(resumed.ok, true);
      assert.deepEqual(restarted.core.publication(), { revision: resumed.revision, mask: MASK.IDLE },
        "a complete broker restart preserves the selection and the same Workspace restores publication");
      assert.ok(resumed.revision > firstRevision);
      assert.equal(restarted.core.selected, `opencode:${activated.groupId}`,
        "the restarted broker still holds the same Selected Agent Source");
    } finally {
      await restarted.close();
    }
    pass("installation validation, fail-open recovery, restricted permissions, and restart-safe selection");
  } finally {
    await broker.close().catch(() => {});
  }
}

async function testActivationRetention() {
  const root = await mkdtemp("/tmp/pomodoro-activation-retention-");
  const workspaceA = path.join(root, "workspace-a");
  const workspaceB = path.join(root, "workspace-b");
  await mkdir(workspaceA);
  await mkdir(workspaceB);
  const stateDir = path.join(root, "pomodoro", "agent-status");
  const env = { ...process.env, XDG_STATE_HOME: root };
  const counter = { toasts: [], triggers: 0, clientTouches: [] };
  const tui = fakeTuiApi({ directory: workspaceA, worktree: workspaceA, counter });
  const groupA = hashId(await realpath(workspaceA));
  let broker = await runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
  try {
    const tuiModule = await import("./opencode-tui.mjs");
    await tuiModule.default.tui(tui.api, { env });
    tui.commands()[0].onSelect();
    await waitFor(() => broker.core.selected === `opencode:${groupA}` && broker.core.revision === 1, 3_000);
    assert.equal(tui.toasts().length, 1);
    const rootA = await realpath(workspaceA);
    await waitFor(() => broker.core.members.get(`opencode:${groupA}`)?.has(hashId(`pomodoro-tui:${rootA}`)), 2_000);

    await broker.close();
    const impostorSockets = new Set();
    const impostor = net.createServer((socket) => {
      impostorSockets.add(socket);
      socket.once("close", () => impostorSockets.delete(socket));
      socket.end("not-json\n");
    });
    await new Promise((resolve) => impostor.listen(broker.socketPath, resolve));
    tui.api.state.path.directory = workspaceB;
    tui.api.state.path.worktree = workspaceB;
    tui.commands()[0].onSelect();
    await waitFor(() => tui.toasts().length === 2);
    assert.equal(tui.toasts()[1].variant, "error", "the failed replacement selection reports bounded failure");
    for (const socket of impostorSockets) socket.destroy();
    await new Promise((resolve) => impostor.close(() => resolve()));

    broker = await runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
    assert.equal(broker.core.selected, `opencode:${groupA}`, "the failed selection did not change the persisted Selected Agent Source");
    await waitFor(() => broker.core.publication()?.mask === MASK.IDLE, MEMBER_LEASE_MS + 4_000,
      "the previous activation keep-alive survives the failed selection and renews past a lease window");
    assert.ok(broker.core.members.get(`opencode:${groupA}`)?.has(hashId(`pomodoro-tui:${rootA}`)),
      "the original activation member is still renewed by its keep-alive");
    assert.equal(broker.core.selected, `opencode:${groupA}`);
    pass("a failed selection keeps the previous activation alive until a replacement succeeds");
  } finally {
    await tui.dispose().catch(() => {});
    await broker?.close().catch(() => {});
  }
}

async function testAtomicActivation() {
  const root = await mkdtemp("/tmp/pomodoro-atomic-activation-");
  const workspaceA = path.join(root, "ws-a");
  const workspaceB = path.join(root, "ws-b");
  await mkdir(workspaceA);
  await mkdir(workspaceB);
  const stateDir = path.join(root, "pomodoro", "agent-status");
  const env = { ...process.env, XDG_STATE_HOME: root };
  const rootA = await realpath(workspaceA);
  const rootB = await realpath(workspaceB);
  const groupA = hashId(rootA);
  const groupB = hashId(rootB);
  let broker = await runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
  const activationA = () => new ActivationSession({ root: rootA, env });
  const activationB = () => new ActivationSession({ root: rootB, env });
  try {
    assert.equal(await (await activationA()).activate().then((r) => r.revision), 1, "fresh unselected source: exactly one advance");
    assert.equal(await (await activationA()).activate().then((r) => r.revision), 2, "live reselect: exactly one advance");

    const invalid = [
      { type: "activate", groupId: groupA, memberId: "m", pid: process.pid, startedAt: Date.now() },
      { type: "activate", groupId: groupA, memberId: "m", pid: process.pid, startedAt: Date.now(), mask: MASK.IDLE, host: "opencode", extra: 1 },
      { type: "activate", groupId: groupA, memberId: "m", pid: 0x7fffffff, startedAt: Date.now(), mask: MASK.IDLE, host: "opencode" },
      { type: "activate", groupId: groupA, memberId: "m", pid: process.pid, startedAt: 0, mask: MASK.IDLE, host: "opencode" },
      { type: "activate", groupId: groupA, memberId: "m", pid: process.pid, startedAt: Date.now(), mask: 16, host: "opencode" },
      { type: "activate", groupId: groupA, memberId: "m", pid: process.pid, startedAt: Date.now(), mask: MASK.IDLE, host: "cursor" },
      { type: "activate", groupId: groupA, pid: process.pid, startedAt: Date.now(), mask: MASK.IDLE, host: "opencode" },
      { type: "activate", groupId: groupA, memberId: "m", pid: process.pid, startedAt: Date.now(), mask: MASK.IDLE, host: 9 },
    ];
    for (const bad of invalid) {
      const reply = await brokerRequest(broker.socketPath, bad);
      assert.equal(reply.ok, false, `invalid activate is rejected: ${JSON.stringify(bad)}`);
    }
    assert.equal(broker.core.revision, 2, "rejected activate requests never advance");

    const codexActivate = await brokerRequest(broker.socketPath, {
      type: "activate", groupId: groupA, memberId: "codex-activator", pid: process.pid, startedAt: Date.now(), mask: MASK.WORKING,
    });
    assert.deepEqual(codexActivate, { ok: true, selected: groupA, revision: 3 },
      "activate without host defaults to Codex and keeps the historical response shape");

    const codexSeeded = await brokerRequest(broker.socketPath, { type: "report", groupId: groupB, memberId: "codex-seed", pid: process.pid, startedAt: Date.now(), mask: MASK.WAITING });
    assert.equal(codexSeeded.ok, true, "the Codex first-valid-source rule still selects Codex");
    const switched = await activationB().activate();
    assert.equal(switched.revision, codexSeeded.revision + 1, "activation while another source is selected costs exactly one advance");
    assert.equal(broker.core.selected, `opencode:${groupB}`);

    const [first, second, third] = await Promise.all([
      activationA().activate(),
      activationA().activate(),
      activationB().activate(),
    ]);
    assert.equal(first.ok !== false, true);
    const revisions = [first.revision, second.revision, third.revision];
    assert.deepEqual(revisions.slice().sort((a, b) => a - b), revisions, "concurrent activations return monotonic revisions");
    assert.equal(new Set(revisions).size, 3, "each concurrent invocation advances exactly once");
    assert.equal(broker.core.revision, Math.max(...revisions), "the broker revision matches the last accepted activation");
    pass("atomic activation: one advance per accepted invocation, serially and concurrently");
  } finally {
    await broker.close().catch(() => {});
  }

  const persistedRevision = JSON.parse(await readFile(path.join(stateDir, "agent-coordination.json"), "utf8")).revision;
  broker = await runBroker({ stateDir, logger: () => {}, publisherOptions: { resolvePort: async () => null } });
  try {
    assert.equal(broker.core.publication(), null, "the restarted broker starts selected but vacant");
    const revived = await activationA().activate();
    assert.equal(revived.revision, persistedRevision + 1,
      "activation after a restart against a persisted selected-but-vacant source advances exactly once");
    assert.deepEqual(broker.core.publication(), { revision: revived.revision, mask: MASK.IDLE },
      "publication resumes with the activated member in the same step");
    assert.equal(broker.core.selected, `opencode:${groupA}`);
    pass("atomic activation restores a persisted selected-vacant Workspace with exactly one revision");
  } finally {
    await broker.close().catch(() => {});
  }

  const staleRoot = path.join(root, "ws-stale");
  await mkdir(staleRoot);
  const staleGroup = hashId(await realpath(staleRoot));
  const staleRootPath = await realpath(staleRoot);
  const staleEnv = { ...process.env, XDG_STATE_HOME: path.join(root, "stale-state") };
  const staleBroker = await runBroker({
    stateDir: path.join(root, "stale-state", "pomodoro", "agent-status"),
    logger: () => {},
    publisherOptions: { resolvePort: async () => null },
    reapMs: 60_000,
  });
  let child = null;
  try {
    // Stage the finding exactly: the selected source holds one live member (the
    // activation member, renewed manually) plus one stale member whose removal
    // changes the aggregate mask.
    const staleSession = new ActivationSession({ root: staleRootPath, env: staleEnv });
    const firstActivation = await staleSession.activate();
    assert.equal(firstActivation.revision, 1);
    staleSession.stop();
    child = await spawnOpenCodeProcess({ socketPath: staleBroker.socketPath, groupId: staleGroup, mask: MASK.WORKING });
    await waitFor(() => staleBroker.core.publication()?.mask === MASK.WORKING, 3_000,
      "the child member contributes Working to the selected aggregate");
    child.kill("SIGKILL");
    // Deterministic window: no source-scoped request runs between the child's death
    // and the activation below. The activation member stays lease-fresh while the
    // dead child member becomes prunable, so the activate's own scoped reap is what
    // removes it (with an aggregate mask change) — the double-advance window.
    await new Promise((resolve) => {
      child.on("exit", resolve);
      setTimeout(resolve, 2_000).unref?.();
    });
    child = null;
    await new Promise((resolve) => setTimeout(resolve, 300));
    const revBefore = staleBroker.core.revision;
    const activated = await new ActivationSession({ root: staleRootPath, env: staleEnv }).activate();
    assert.equal(activated.revision, revBefore + 1,
      "one activate request yields exactly +1 even though its scoped reap removes a stale selected-source member");
    assert.deepEqual(staleBroker.core.publication(), { revision: activated.revision, mask: MASK.IDLE },
      "the aggregate after activation reflects only the refreshed activation member");
    assert.equal(staleBroker.core.members.get(`opencode:${staleGroup}`).size, 1,
      "the stale member was removed by the same activation step");
    assert.equal(staleBroker.core.selected, `opencode:${staleGroup}`);
    pass("activation cleans stale selected-source members and still advances exactly once");
  } finally {
    if (child) child.kill("SIGKILL");
    await staleBroker.close().catch(() => {});
  }
}

await testPureCore();
await testHookSelection();
await testReporterIntegration();
await testOpenCodeActivation();
await testAtomicActivation();
await testOpenCodePluginSelection();
await testActivationRetention();
await testOpenCodeLifecycle();
await testCrossHostPluginIsolation();
await testInstallationAndRecovery();
await testUnselectedExpiryIsolation();
await testLegacyPersistedSelection();
await testBrokerRecoveryAndFailures();
await testOnDemandBroker();
console.log(`pomodoro-agent-status check: ${checks} checks passed`);
