#!/usr/bin/env node

import { constants as fsConstants } from "node:fs";
import { spawnSync } from "node:child_process";
import { chmod, lstat, mkdir, open, readFile, readdir, rename, unlink, writeFile } from "node:fs/promises";
import net from "node:net";
import os from "node:os";
import path from "node:path";
import process from "node:process";
import { pathToFileURL } from "node:url";

export const MAX_LINE_BYTES = 8 * 1024;
export const MEMBER_LEASE_MS = 10_000;
export const HEARTBEAT_MS = 3_000;
export const RETRY_MS = 1_000;
export const MAX_REVISION = 0xffff_ffff;

const ID_MAX_LENGTH = 128;
const O_EXLOCK = 0x20;
const HOSTS = new Set(["codex", "opencode"]);
const REQUEST_FIELDS = {
  report: ["type", "groupId", "memberId", "pid", "startedAt", "mask"],
  select: ["type", "groupId"],
  remove: ["type", "groupId", "memberId"],
  activate: ["type", "groupId", "memberId", "pid", "startedAt", "mask"],
};

function exactFields(value, expected) {
  const actual = Object.keys(value).sort();
  return actual.length === expected.length &&
    actual.every((field, index) => field === [...expected].sort()[index]);
}

function sourceKey(host, groupId) {
  return `${host}:${groupId}`;
}

function selectionState(key) {
  if (key === null) return { selected: null, selectedHost: null };
  const separator = key.indexOf(":");
  return { selected: key.slice(separator + 1), selectedHost: key.slice(0, separator) };
}

function validId(value) {
  return typeof value === "string" && value.length > 0 && value.length <= ID_MAX_LENGTH;
}

export function pidIsAlive(pid) {
  if (!Number.isSafeInteger(pid) || pid <= 0) return false;
  try {
    process.kill(pid, 0);
    return true;
  } catch (error) {
    return error?.code === "EPERM";
  }
}

export function validateRequest(value, isAlive = pidIsAlive) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error("request must be an object");
  }
  const expected = REQUEST_FIELDS[value.type];
  if (!expected) throw new Error("unknown request type");
  if ("host" in value && !HOSTS.has(value.host)) throw new Error("invalid host");
  if (!exactFields(value, "host" in value ? [...expected, "host"] : expected)) {
    throw new Error("request fields do not match type");
  }
  if (!validId(value.groupId)) throw new Error("invalid groupId");
  if (value.type !== "select" && !validId(value.memberId)) {
    throw new Error("invalid memberId");
  }
  if (value.type === "report" || value.type === "activate") {
    if (!Number.isSafeInteger(value.pid) || value.pid <= 0 || !isAlive(value.pid)) {
      throw new Error("pid is not alive");
    }
    if (!Number.isSafeInteger(value.startedAt) || value.startedAt <= 0) {
      throw new Error("invalid startedAt");
    }
    if (!Number.isInteger(value.mask) || value.mask < 0 || value.mask > 15) {
      throw new Error("invalid mask");
    }
  }
  return value;
}

function response(core, ok, error) {
  const selection = selectionState(core.selected);
  return {
    ok,
    selected: selection.selected,
    revision: core.revision,
    ...(selection.selectedHost === "opencode" ? { selectedHost: selection.selectedHost } : {}),
    ...(error ? { error } : {}),
  };
}

export class BrokerCore {
  constructor({ selected = null, revision = 0, now = Date.now, isAlive = pidIsAlive, persist = async () => {} } = {}) {
    this.selected = selected;
    this.revision = revision;
    this.now = now;
    this.isAlive = isAlive;
    this.persist = persist;
    this.members = new Map();
    this.failed = false;
    this.queue = Promise.resolve();
  }

  enqueue(operation) {
    const result = this.queue.then(operation);
    this.queue = result.catch(() => {});
    return result;
  }

  reap() {
    return this.enqueue(() => this.reapNow());
  }

  async reapNow(scope = null) {
    const now = this.now();
    const selectedKey = this.selected;
    const oldMask = this.selected ? this.sourceMask(this.selected) : 0;
    const survivors = new Map();
    for (const [key, members] of this.members) {
      if (scope !== null && key !== scope) {
        survivors.set(key, members);
        continue;
      }
      const live = new Map();
      for (const [memberId, member] of members) {
        if (now - member.lastSeen < MEMBER_LEASE_MS && this.isAlive(member.pid)) live.set(memberId, member);
      }
      if (live.size > 0) survivors.set(key, live);
    }
    const selectedSurvivors = selectedKey ? survivors.get(selectedKey) : null;
    let nextMask = 0;
    for (const member of selectedSurvivors?.values() ?? []) nextMask |= member.mask;
    if (selectedSurvivors && nextMask !== oldMask) await this.advance();
    this.members = survivors;
  }

  sourceMask(groupId) {
    let mask = 0;
    for (const member of this.members.get(groupId)?.values() ?? []) mask |= member.mask;
    return mask;
  }

  sourceIsLive(groupId) {
    return (this.members.get(groupId)?.size ?? 0) > 0;
  }

  publication() {
    if (this.failed || !this.selected || !this.sourceIsLive(this.selected)) return null;
    return { revision: this.revision, mask: this.sourceMask(this.selected) };
  }

  async advance(selected = this.selected) {
    try {
      if (this.revision >= MAX_REVISION) throw new Error("revision exhausted");
      const revision = this.revision + 1;
      await this.persist({ ...selectionState(selected), revision });
      this.selected = selected;
      this.revision = revision;
    } catch (error) {
      this.failed = true;
      throw error;
    }
  }

  handle(raw) {
    return this.enqueue(() => this.handleOne(raw));
  }

  async handleOne(raw) {
    let request;
    try {
      request = validateRequest(raw, this.isAlive);
      if (this.failed) throw new Error("broker state unavailable");
      const host = request.host ?? "codex";
      const source = sourceKey(host, request.groupId);
      await this.reapNow(source);

      if (request.type === "select") {
        if (!this.sourceIsLive(source)) throw new Error("source has no live members");
        await this.advance(source);
      } else if (request.type === "activate") {
        // One serialized operation: refresh the activation member and perform the
        // explicit selection, so every accepted activation costs exactly one advance
        // regardless of prior state (fresh, live reselect, or selected-but-vacant).
        await this.advance(source);
        const members = this.members.get(source) ?? new Map();
        members.set(request.memberId, { ...request, lastSeen: this.now() });
        this.members.set(source, members);
      } else if (request.type === "remove") {
        const members = this.members.get(source);
        const oldMask = this.sourceMask(source);
        let nextMask = 0;
        let remaining = 0;
        for (const [memberId, member] of members ?? []) {
          if (memberId !== request.memberId) {
            nextMask |= member.mask;
            remaining++;
          }
        }
        if (this.selected === source && remaining > 0 && nextMask !== oldMask) await this.advance();
        members?.delete(request.memberId);
        if (members?.size === 0) this.members.delete(source);
      } else {
        const members = this.members.get(source) ?? new Map();
        const wasLive = members.size > 0;
        const oldMask = this.sourceMask(source);
        const prior = members.get(request.memberId);
        let nextMask = request.mask;
        for (const [memberId, member] of members) {
          if (memberId !== request.memberId) nextMask |= member.mask;
        }
        const firstSelection = this.selected === null && host === "codex";
        const selectedRecovery = this.selected === source && !wasLive;
        const selectedMaskChange = this.selected === source && wasLive && oldMask !== nextMask;
        if (firstSelection) await this.advance(source);
        else if (selectedRecovery || selectedMaskChange) await this.advance();
        members.set(request.memberId, { ...request, lastSeen: this.now() });
        this.members.set(source, members);
      }
      return response(this, true);
    } catch (error) {
      return response(this, false, error.message);
    }
  }
}

function validatePersistedState(value) {
  if (!value || typeof value !== "object" || Array.isArray(value) ||
      !Number.isInteger(value.revision) || value.revision < 0 || value.revision > MAX_REVISION) {
    throw new Error("invalid agent-coordination.json");
  }
  const legacy = exactFields(value, ["selected", "revision"]);
  const qualified = exactFields(value, ["selected", "selectedHost", "revision"]);
  if (!legacy && !qualified) throw new Error("invalid agent-coordination.json");
  if (value.selected !== null && !validId(value.selected)) {
    throw new Error("invalid agent-coordination.json");
  }
  const host = legacy && value.selected !== null ? "codex" : qualified ? value.selectedHost : null;
  if ((value.selected === null) !== (host === null) || (host !== null && !HOSTS.has(host))) {
    throw new Error("invalid agent-coordination.json");
  }
  return {
    selected: value.selected === null ? null : sourceKey(host, value.selected),
    revision: value.revision,
  };
}

export async function prepareState(stateDir) {
  await mkdir(stateDir, { recursive: true, mode: 0o700 });
  await chmod(stateDir, 0o700);
  const statePath = path.join(stateDir, "agent-coordination.json");
  let saved = { selected: null, revision: 0 };
  try {
    const contents = await readFile(statePath, "utf8");
    try {
      saved = validatePersistedState(JSON.parse(contents));
    } catch (error) {
      if (error.message === "invalid agent-coordination.json") throw error;
      throw new Error("invalid agent-coordination.json", { cause: error });
    }
  } catch (error) {
    if (error?.code !== "ENOENT") throw error;
  }
  const persist = async (value) => {
    const temporary = `${statePath}.${process.pid}.${Date.now()}.tmp`;
    try {
      await writeFile(temporary, `${JSON.stringify(value)}\n`, { mode: 0o600, flag: "wx" });
      await rename(temporary, statePath);
      await chmod(statePath, 0o600);
    } catch (error) {
      await unlink(temporary).catch(() => {});
      throw error;
    }
  };
  return { ...saved, persist, statePath };
}

export function reservationIsActive(reservationPath, isAlive = pidIsAlive) {
  return readFile(reservationPath, "utf8")
    .then((contents) => isAlive(Number(contents.trim())))
    .catch((error) => {
      if (error?.code === "ENOENT") return false;
      throw error;
    });
}

export async function discoverPort(explicit = process.env.POMODORO_PORT) {
  if (explicit) return explicit;
  const candidates = (await readdir("/dev")).filter((name) => name.startsWith("cu.usbmodem"));
  return candidates.length === 1 ? path.join("/dev", candidates[0]) : null;
}

export async function openSerialTransport(port, onLine, onDisconnect) {
  spawnSync("stty", ["-f", port, "raw", "-echo"], { stdio: "ignore" });
  const handle = await open(port, fsConstants.O_RDWR | fsConstants.O_NONBLOCK | O_EXLOCK);
  let buffered = "";
  let reading = false;
  let closed = false;
  const chunk = Buffer.alloc(256);
  const poll = async () => {
    if (reading || closed) return;
    reading = true;
    try {
      const { bytesRead } = await handle.read(chunk, 0, chunk.length, null);
      if (bytesRead === 0) return;
      buffered += chunk.toString("utf8", 0, bytesRead);
    } catch (error) {
      if (!["EAGAIN", "EWOULDBLOCK"].includes(error.code)) onDisconnect(error);
      return;
    } finally {
      reading = false;
    }
    for (;;) {
      const newline = buffered.indexOf("\n");
      if (newline < 0) break;
      const line = buffered.slice(0, newline).replace(/\r$/, "");
      buffered = buffered.slice(newline + 1);
      if (Buffer.byteLength(line) <= 64) onLine(line);
    }
    if (Buffer.byteLength(buffered) > 64) buffered = "";
  };
  const timer = setInterval(() => void poll(), 50);
  timer.unref?.();
  return {
    write: (line) => handle.write(`${line}\n`),
    close: async () => {
      closed = true;
      clearInterval(timer);
      await handle.close().catch(() => {});
    },
  };
}

export class SerialPublisher {
  constructor({ publication, reserved, resolvePort = discoverPort, connect = openSerialTransport,
                now = Date.now, retryMs = RETRY_MS, heartbeatMs = HEARTBEAT_MS,
                onAck = () => {}, onError = () => {} }) {
    this.publication = publication;
    this.reserved = reserved;
    this.resolvePort = resolvePort;
    this.connect = connect;
    this.now = now;
    this.retryMs = retryMs;
    this.heartbeatMs = heartbeatMs;
    this.onAck = onAck;
    this.onError = onError;
    this.lastError = null;
    this.transport = null;
    this.enabled = false;
    this.nextRetry = 0;
    this.nextHeartbeat = 0;
  }

  reportError(error) {
    const message = error?.message ?? String(error);
    if (message !== this.lastError) this.onError(message);
    this.lastError = message;
  }

  async close() {
    const transport = this.transport;
    this.transport = null;
    this.enabled = false;
    await transport?.close();
  }

  async disconnected(error) {
    if (error) this.reportError(error);
    await this.close();
    this.nextRetry = this.now() + this.retryMs;
  }

  async send() {
    const current = this.publication();
    if (!this.transport || !this.enabled || !current) return;
    try {
      await this.transport.write(`OC1 STATE ${current.revision} ${current.mask}`);
      this.nextHeartbeat = this.now() + this.heartbeatMs;
    } catch (error) {
      await this.disconnected(error);
    }
  }

  receive(line) {
    if (line === "OC1 ENABLED") {
      this.enabled = true;
      void this.send();
      return;
    }
    if (line === "OC1 DISABLED") {
      this.enabled = false;
      return;
    }
    const match = /^OC1 ACK (0|[1-9][0-9]*) (1|8)$/.exec(line);
    const current = this.publication();
    if (match && current && Number(match[1]) === current.revision && (current.mask & Number(match[2])) !== 0) {
      this.onAck({ revision: current.revision, mask: Number(match[2]) });
    }
  }

  async tick() {
    let shouldClose = !this.publication();
    try {
      shouldClose ||= await this.reserved();
    } catch (error) {
      this.reportError(error);
      shouldClose = true;
    }
    if (shouldClose) {
      await this.close();
      return;
    }
    if (!this.transport) {
      if (this.now() < this.nextRetry) return;
      try {
        const port = await this.resolvePort();
        if (!port) throw new Error("no unambiguous serial port");
        this.transport = await this.connect(port, (line) => this.receive(line), (error) => void this.disconnected(error));
        this.lastError = null;
        this.nextHeartbeat = this.now();
      } catch (error) {
        this.reportError(error);
        await this.close();
        this.nextRetry = this.now() + this.retryMs;
      }
      return;
    }
    if (this.enabled && this.now() >= this.nextHeartbeat) await this.send();
  }
}

function writeResponse(socket, value) {
  if (!socket.destroyed) socket.write(`${JSON.stringify(value)}\n`);
}

export function handleConnection(socket, core, changed = () => {}) {
  let buffer = Buffer.alloc(0);
  let discarding = false;
  let queue = Promise.resolve();
  socket.on("data", (chunk) => {
    buffer = Buffer.concat([buffer, chunk]);
    for (;;) {
      const newline = buffer.indexOf(10);
      if (newline < 0) {
        if (buffer.length > MAX_LINE_BYTES && !discarding) {
          writeResponse(socket, response(core, false, "request line exceeds 8 KiB"));
          discarding = true;
        }
        return;
      }
      const line = buffer.subarray(0, newline);
      buffer = buffer.subarray(newline + 1);
      if (discarding || line.length > MAX_LINE_BYTES) {
        if (!discarding) writeResponse(socket, response(core, false, "request line exceeds 8 KiB"));
        discarding = false;
        continue;
      }
      queue = queue.then(async () => {
        let request;
        try {
          request = JSON.parse(line.toString("utf8"));
        } catch {
          writeResponse(socket, response(core, false, "invalid JSON"));
          return;
        }
        const result = await core.handle(request);
        writeResponse(socket, result);
        if (result.ok) changed(request);
      });
    }
  });
}

async function socketHasListener(socketPath) {
  return new Promise((resolve, reject) => {
    const client = net.createConnection(socketPath);
    client.once("connect", () => { client.destroy(); resolve(true); });
    client.once("error", (error) => {
      if (["ECONNREFUSED", "ENOENT", "ENOTSOCK"].includes(error.code)) resolve(false);
      else reject(error);
    });
  });
}

export async function listenExclusive(server, socketPath) {
  const listen = () => new Promise((resolve, reject) => {
    const onError = (error) => { server.off("listening", onListening); reject(error); };
    const onListening = () => { server.off("error", onError); resolve(); };
    server.once("error", onError);
    server.once("listening", onListening);
    server.listen(socketPath);
  });
  try {
    await listen();
  } catch (error) {
    if (error.code !== "EADDRINUSE") throw error;
    const occupied = await lstat(socketPath).catch(() => null);
    if (!occupied?.isSocket()) throw new Error("socket path exists and is not a socket");
    if (await socketHasListener(socketPath)) throw new Error("broker is already running");
    await unlink(socketPath);
    await listen();
  }
  try {
    await chmod(socketPath, 0o600);
  } catch (error) {
    await new Promise((resolve) => server.close(resolve));
    await unlink(socketPath).catch(() => {});
    throw error;
  }
}

export function defaultStateDir(env = process.env) {
  const root = env.XDG_STATE_HOME || (env.HOME && path.join(env.HOME, ".local", "state"));
  if (!root) throw new Error("cannot determine user state directory");
  return path.join(root, "pomodoro", "agent-status");
}

export async function runBroker({ stateDir = defaultStateDir(), logger = console.error, publisherOptions = {}, idleMs = null } = {}) {
  const prepared = await prepareState(stateDir);
  const core = new BrokerCore(prepared);
  const socketPath = path.join(stateDir, "agent-status.sock");
  const publisher = new SerialPublisher({
    publication: () => core.publication(),
    reserved: () => reservationIsActive(path.join(stateDir, "flash-reservation")),
    onError: (message) => logger(`broker: serial unavailable; retrying (${message})`),
    ...publisherOptions,
  });
  const sockets = new Set();
  let lastReportAt = Date.now();
  const server = net.createServer((socket) => {
    sockets.add(socket);
    socket.once("close", () => sockets.delete(socket));
    handleConnection(socket, core, (request) => {
      if (request.type === "report") lastReportAt = Date.now();
      void publisher.tick();
    });
  });
  await listenExclusive(server, socketPath);
  let ticking = false;
  let closing;
  let resolveClosed;
  const closed = new Promise((resolve) => { resolveClosed = resolve; });
  const close = () => closing ??= (async () => {
    try {
      clearInterval(timer);
      await publisher.close();
      for (const socket of sockets) socket.destroy();
      await new Promise((resolve) => server.close(resolve));
      await unlink(socketPath).catch((error) => { if (error.code !== "ENOENT") throw error; });
    } finally {
      resolveClosed();
    }
  })();
  const timer = setInterval(() => {
    if (ticking) return;
    ticking = true;
    core.reap()
      .catch((error) => logger(`broker: ${error.message}`))
      .then(() => publisher.tick())
      .then(() => {
        if (idleMs !== null && Date.now() - lastReportAt >= idleMs) return close();
      })
      .catch((error) => logger(`broker: ${error.message}`))
      .finally(() => { ticking = false; });
  }, 250);
  timer.unref?.();
  await publisher.tick();
  return {
    core,
    socketPath,
    close,
    closed,
  };
}

export async function runBrokerCli(args = process.argv.slice(2)) {
  if (args.some((arg) => arg !== "--idle")) {
    console.error("usage: pomodoro-agent-status-broker.mjs [--idle]");
    process.exitCode = 2;
    return;
  }
  let broker;
  try {
    broker = await runBroker({ idleMs: args.includes("--idle") ? MEMBER_LEASE_MS : null });
  } catch (error) {
    console.error(`broker: ${error.message}`);
    process.exitCode = 1;
    return;
  }
  const stop = async () => {
    process.removeAllListeners("SIGINT");
    process.removeAllListeners("SIGTERM");
    await broker.close();
  };
  process.once("SIGINT", () => void stop().then(() => process.exit(130)));
  process.once("SIGTERM", () => void stop().then(() => process.exit(143)));
  await broker.closed;
  process.removeAllListeners("SIGINT");
  process.removeAllListeners("SIGTERM");
}

if (process.argv[1] && pathToFileURL(path.resolve(process.argv[1])).href === import.meta.url) await runBrokerCli();
