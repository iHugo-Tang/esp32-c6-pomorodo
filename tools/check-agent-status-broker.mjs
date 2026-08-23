#!/usr/bin/env node

import assert from "node:assert/strict";
import { chmod, mkdtemp, readFile, stat, unlink, writeFile } from "node:fs/promises";
import net from "node:net";
import os from "node:os";
import path from "node:path";
import { spawn } from "node:child_process";
import {
  BrokerCore,
  HEARTBEAT_MS,
  MAX_LINE_BYTES,
  MAX_REVISION,
  MEMBER_LEASE_MS,
  SerialPublisher,
  handleConnection,
  prepareState,
  runBroker,
  validateRequest,
} from "./pomodoro-agent-status-broker.mjs";

let checks = 0;
function pass(name) {
  checks++;
  console.log(`ok ${checks}: ${name}`);
}

function report(overrides = {}) {
  return {
    type: "report", client: "opencode", groupId: "group-a", memberId: "one",
    pid: process.pid, startedAt: 1_700_000_000_000, mask: 1, ...overrides,
  };
}

async function request(socketPath, value) {
  return new Promise((resolve, reject) => {
    const socket = net.createConnection(socketPath);
    let data = "";
    socket.setEncoding("utf8");
    socket.once("error", reject);
    socket.on("data", (chunk) => {
      data += chunk;
      const newline = data.indexOf("\n");
      if (newline >= 0) {
        socket.destroy();
        resolve(JSON.parse(data.slice(0, newline)));
      }
    });
    socket.once("connect", () => socket.write(`${typeof value === "string" ? value : JSON.stringify(value)}\n`));
  });
}

async function waitFor(check, timeout = 2_000) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) {
    if (await check()) return;
    await new Promise((resolve) => setTimeout(resolve, 20));
  }
  throw new Error("timed out waiting for condition");
}

async function testValidationAndCore() {
  const alive = new Set([10, 11, 12]);
  let now = 100_000;
  const persisted = [];
  const core = new BrokerCore({
    now: () => now,
    isAlive: (pid) => alive.has(pid),
    persist: async (state) => persisted.push(structuredClone(state)),
  });
  assert.throws(() => validateRequest({ ...report({ pid: 10 }), extra: true }, () => true));
  assert.throws(() => validateRequest(report({ pid: 99 }), (pid) => alive.has(pid)));
  assert.throws(() => validateRequest(report({ mask: 16 }), () => true));
  assert.throws(() => validateRequest({ type: "wat" }, () => true));

  let result = await core.handle(report({ pid: 10 }));
  assert.equal(result.ok, true);
  assert.deepEqual(result.selected, { client: "opencode", groupId: "group-a" });
  assert.equal(result.revision, 1);
  assert.deepEqual(core.publication(), { revision: 1, mask: 1 });

  result = await core.handle(report({ pid: 10 }));
  assert.equal(result.revision, 1, "repeat report is a lease heartbeat");
  await core.handle(report({ pid: 11, memberId: "two", mask: 4 }));
  assert.deepEqual(core.publication(), { revision: 2, mask: 5 });
  await core.handle(report({ pid: 10, mask: 4 }));
  assert.deepEqual(core.publication(), { revision: 3, mask: 4 }, "shared bits remain aggregated");

  await core.handle(report({ client: "codex", groupId: "workspace", memberId: "agent", pid: 12, mask: 8 }));
  assert.deepEqual(core.publication(), { revision: 3, mask: 4 }, "sources stay isolated");
  result = await core.handle({ type: "select", client: "codex", groupId: "workspace" });
  assert.equal(result.revision, 4);
  result = await core.handle({ type: "select", client: "codex", groupId: "workspace" });
  assert.equal(result.revision, 5, "repeat select opens a revision");

  alive.delete(12);
  await core.reap();
  assert.equal(core.publication(), null);
  assert.deepEqual(core.selected, { client: "codex", groupId: "workspace" }, "selection is retained");
  assert.equal((await core.handle({ type: "select", client: "opencode", groupId: "missing" })).ok, false);
  assert.equal(core.publication(), null, "another source cannot take over automatically");

  alive.add(12);
  result = await core.handle(report({ client: "codex", groupId: "workspace", memberId: "agent", pid: 12, mask: 8 }));
  assert.equal(result.revision, 6, "selected source recovery opens a revision");
  now += MEMBER_LEASE_MS;
  await core.reap();
  assert.equal(core.publication(), null, "lease expiry removes members");
  assert.deepEqual(persisted.at(-1), { selected: { client: "codex", groupId: "workspace" }, revision: 6 });

  const removals = new BrokerCore({ now: () => now, isAlive: () => true });
  await removals.handle(report({ pid: 10, mask: 1 }));
  await removals.handle(report({ pid: 11, memberId: "zero", mask: 0 }));
  result = await removals.handle({ type: "remove", client: "opencode", groupId: "group-a", memberId: "one" });
  assert.equal(result.revision, 2);
  assert.deepEqual(removals.publication(), { revision: 2, mask: 0 });

  const raced = new BrokerCore({
    isAlive: () => true,
    persist: () => new Promise((resolve) => setTimeout(resolve, 10)),
  });
  await Promise.all([
    raced.handle(report({ pid: 10 })),
    raced.handle(report({ client: "codex", groupId: "workspace", memberId: "agent", pid: 11 })),
  ]);
  assert.deepEqual(raced.selected, { client: "opencode", groupId: "group-a" });
  assert.equal(raced.revision, 1, "concurrent first reports serialize without stealing");

  const exhausted = new BrokerCore({ revision: MAX_REVISION, isAlive: () => true });
  result = await exhausted.handle(report({ pid: 10 }));
  assert.equal(result.ok, false);
  assert.match(result.error, /revision exhausted/);
  assert.equal(exhausted.publication(), null, "revision exhaustion fails closed");
  pass("validation, aggregation, leases, isolation, selection, recovery, and revision rules");
}

async function testSocketAndPersistence() {
  const root = await mkdtemp(path.join(os.tmpdir(), "pomodoro-broker-"));
  const options = { stateDir: root, logger: () => {}, publisherOptions: { resolvePort: async () => null } };
  const broker = await runBroker(options);
  try {
    assert.equal((await stat(root)).mode & 0o777, 0o700);
    assert.equal((await stat(broker.socketPath)).mode & 0o777, 0o600);
    assert.equal((await request(broker.socketPath, "{" )).ok, false);
    assert.equal((await request(broker.socketPath, "x".repeat(MAX_LINE_BYTES + 1))).ok, false);
    const first = await request(broker.socketPath, report());
    assert.equal(first.revision, 1);
    const parallel = await Promise.all([
      request(broker.socketPath, report({ memberId: "two", mask: 2 })),
      request(broker.socketPath, report({ client: "codex", groupId: "other", memberId: "x", mask: 4 })),
    ]);
    assert.equal(parallel.every((item) => item.ok), true);
    const saved = JSON.parse(await readFile(path.join(root, "agent-coordination.json"), "utf8"));
    assert.deepEqual(Object.keys(saved).sort(), ["revision", "selected"]);
    assert.equal(saved.revision, 2);
    assert.equal((await stat(path.join(root, "agent-coordination.json"))).mode & 0o777, 0o600);
    await assert.rejects(runBroker(options), /already running/);
  } finally {
    await broker.close();
  }

  const restarted = await runBroker(options);
  try {
    assert.equal(restarted.core.revision, 2);
    assert.equal(restarted.core.publication(), null, "members are not restored");
    const recovered = await request(restarted.socketPath, report());
    assert.equal(recovered.revision, 3);
  } finally {
    await restarted.close();
  }

  const staleSocket = path.join(root, "agent-status.sock");
  const staleOwner = spawn(process.execPath, ["-e", "const net=require('net');net.createServer().listen(process.argv[1],()=>console.log('ready'))", staleSocket]);
  await new Promise((resolve) => staleOwner.stdout.once("data", resolve));
  staleOwner.kill("SIGKILL");
  await new Promise((resolve) => staleOwner.once("exit", resolve));
  const staleRecovered = await runBroker(options);
  await staleRecovered.close();

  await writeFile(staleSocket, "not a socket");
  await assert.rejects(runBroker(options), /not a socket/);
  assert.equal(await readFile(staleSocket, "utf8"), "not a socket");
  await unlink(staleSocket);

  await writeFile(path.join(root, "agent-coordination.json"), "broken\n");
  await assert.rejects(runBroker(options), /invalid agent-coordination/);
  assert.equal(await readFile(path.join(root, "agent-coordination.json"), "utf8"), "broken\n");
  pass("socket limits, permissions, single instance, stale socket, atomic state, restart, and corrupt-state fail closed");
}

async function testSerialPublisher() {
  let now = 0;
  let publication = { revision: 7, mask: 9 };
  let reserved = false;
  let connects = 0;
  let lineHandler;
  let disconnectHandler;
  const writes = [];
  const acknowledgements = [];
  const transports = [];
  const publisher = new SerialPublisher({
    publication: () => publication,
    reserved: async () => reserved,
    resolvePort: async () => "/tmp/fake-port",
    connect: async (_port, onLine, onDisconnect) => {
      connects++;
      if (connects === 1) throw new Error("occupied");
      lineHandler = onLine;
      disconnectHandler = onDisconnect;
      const transport = {
        closed: false,
        write: async (line) => writes.push(line),
        close: async () => { transport.closed = true; },
      };
      transports.push(transport);
      return transport;
    },
    now: () => now,
    retryMs: 100,
    heartbeatMs: HEARTBEAT_MS,
    onAck: (ack) => acknowledgements.push(ack),
  });

  await publisher.tick();
  assert.equal(connects, 1);
  now = 99;
  await publisher.tick();
  assert.equal(connects, 1, "retry is bounded");
  now = 100;
  await publisher.tick();
  lineHandler("OC1 ENABLED");
  await new Promise((resolve) => setImmediate(resolve));
  assert.deepEqual(writes, ["OC1 STATE 7 9"]);
  now += HEARTBEAT_MS;
  await publisher.tick();
  assert.deepEqual(writes, ["OC1 STATE 7 9", "OC1 STATE 7 9"]);
  lineHandler("OC1 ACK 6 1");
  lineHandler("OC1 ACK 7 2");
  lineHandler("OC1 ACK 7 1");
  assert.deepEqual(acknowledgements, [{ revision: 7, mask: 1 }]);
  lineHandler("OC1 DISABLED");
  now += HEARTBEAT_MS;
  await publisher.tick();
  assert.equal(writes.length, 2);
  reserved = true;
  await publisher.tick();
  assert.equal(transports[0].closed, true);
  reserved = false;
  now += 100;
  await publisher.tick();
  assert.equal(connects, 3, "reservation release reconnects");
  disconnectHandler(new Error("gone"));
  await new Promise((resolve) => setImmediate(resolve));
  now += 100;
  await publisher.tick();
  assert.equal(connects, 4, "disconnect reconnects");
  publication = null;
  await publisher.tick();
  assert.equal(transports.at(-1).closed, true, "missing selected source releases serial");
  pass("serial enable/disable, heartbeat, ACK, retry, reservation, disconnect, and source loss");
}

async function testCliSignalsAndCorruptExit() {
  const root = await mkdtemp("/tmp/pb-cli-");
  const script = path.resolve("tools/pomodoro-agent-status-broker.mjs");
  const child = spawn(process.execPath, [script], { env: { ...process.env, XDG_STATE_HOME: root, POMODORO_PORT: "/missing" } });
  const socketPath = path.join(root, "opencode", "pomodoro-status", "agent-status.sock");
  await waitFor(async () => stat(socketPath).then(() => true).catch(() => false));
  await request(socketPath, report());
  child.kill("SIGTERM");
  const exit = await new Promise((resolve) => child.once("exit", (code, signal) => resolve({ code, signal })));
  assert.equal(exit.code, 143);
  assert.equal(await stat(socketPath).then(() => true).catch(() => false), false);
  const statePath = path.join(root, "opencode", "pomodoro-status", "agent-coordination.json");
  assert.equal(JSON.parse(await readFile(statePath, "utf8")).revision, 1, "selection survives shutdown");

  await writeFile(statePath, "bad");
  const failed = spawn(process.execPath, [script], { env: { ...process.env, XDG_STATE_HOME: root } });
  let stderr = "";
  failed.stderr.setEncoding("utf8");
  failed.stderr.on("data", (chunk) => { stderr += chunk; });
  const failedCode = await new Promise((resolve) => failed.once("exit", resolve));
  assert.equal(failedCode, 1);
  assert.match(stderr, /invalid agent-coordination/);
  pass("CLI SIGTERM cleanup and corrupt-state diagnostic");
}

await testValidationAndCore();
await testSocketAndPersistence();
await testSerialPublisher();
await testCliSignalsAndCorruptExit();
console.log(`agent-status-broker check: ${checks} checks passed`);
