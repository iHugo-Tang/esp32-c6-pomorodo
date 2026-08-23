#!/usr/bin/env node

import { execFileSync, spawn } from "node:child_process";
import { createHash } from "node:crypto";
import { chmod, lstat, mkdir, realpath, unlink } from "node:fs/promises";
import net from "node:net";
import os from "node:os";
import path from "node:path";
import process from "node:process";
import { pathToFileURL } from "node:url";

export const MASK = Object.freeze({ IDLE: 0, ERROR: 1, WAITING: 2, WORKING: 4, COMPLETED: 8 });
export const REPORT_INTERVAL_MS = 3_000;
export const MAX_LINE_BYTES = 8 * 1024;
const CONTROL_PROMPT = "$pomodoro-select";
const EVENTS = new Set([
  "SessionStart", "SessionEnd", "UserPromptSubmit", "PermissionRequest",
  "PreToolUse", "PostToolUse", "SubagentStart", "SubagentStop", "Stop",
]);

export function hashId(value) {
  return createHash("sha256").update(value).digest("hex");
}

export async function workspaceRoot(cwd, gitRoot = defaultGitRoot) {
  const resolved = await realpath(cwd);
  try {
    return await realpath(gitRoot(resolved));
  } catch {
    return resolved;
  }
}

function defaultGitRoot(cwd) {
  return execFileSync("git", ["-C", cwd, "rev-parse", "--show-toplevel"], {
    encoding: "utf8", stdio: ["ignore", "pipe", "ignore"], timeout: 1_000,
  }).trim();
}

export function reporterSocketPath(sessionId, uid = process.getuid?.() ?? 0) {
  return path.join("/tmp", `pomodoro-codex-${uid}`, `${hashId(sessionId).slice(0, 24)}.sock`);
}

export function brokerSocketPath(env = process.env) {
  const base = env.XDG_STATE_HOME || (env.HOME && path.join(env.HOME, ".local", "state"));
  return base ? path.join(base, "opencode", "pomodoro-status", "agent-status.sock") : null;
}

export function validateHookInput(value) {
  if (!value || typeof value !== "object" || Array.isArray(value) ||
      typeof value.session_id !== "string" || !value.session_id ||
      typeof value.cwd !== "string" || !value.cwd ||
      !EVENTS.has(value.hook_event_name)) throw new Error("invalid hook input");
  const event = value.hook_event_name;
  if (event === "SessionStart" && !["startup", "resume", "clear", "compact"].includes(value.source)) throw new Error("invalid SessionStart input");
  if (event === "UserPromptSubmit" && typeof value.prompt !== "string") throw new Error("invalid UserPromptSubmit input");
  if (["PreToolUse", "PostToolUse", "PermissionRequest"].includes(event) && typeof value.tool_name !== "string") throw new Error(`invalid ${event} input`);
  if (event === "PostToolUse" && !("tool_response" in value)) throw new Error("invalid PostToolUse input");
  if (["SubagentStart", "SubagentStop"].includes(event) && typeof value.agent_id !== "string") throw new Error(`invalid ${event} input`);
  return value;
}

export function structuredFailure(value, seen = new Set()) {
  if (!value || typeof value !== "object" || seen.has(value)) return false;
  seen.add(value);
  if (value.isError === true || (Number.isInteger(value.exit_code) && value.exit_code !== 0)) return true;
  return Object.values(value).some((item) => structuredFailure(item, seen));
}

export class StatusState {
  constructor() {
    this.main = MASK.IDLE;
    this.errorLatched = false;
    this.subagents = new Map();
  }

  apply(input) {
    const event = input.hook_event_name;
    if (event === "SessionStart") {
      if (input.source !== "compact") {
        this.main = MASK.IDLE;
        this.errorLatched = false;
        this.subagents.clear();
      }
    } else if (event === "UserPromptSubmit") {
      if (input.prompt.trim() === CONTROL_PROMPT) return { control: true };
      this.work();
    } else if (event === "PermissionRequest") {
      this.main = MASK.WAITING;
    } else if (event === "PreToolUse") {
      if (input.tool_name === "request_user_input") this.main = MASK.WAITING;
      else this.work();
    } else if (event === "PostToolUse") {
      if (structuredFailure(input.tool_response)) {
        this.errorLatched = true;
        this.main = MASK.ERROR;
      } else {
        this.errorLatched = false;
        this.main = MASK.WORKING;
      }
    } else if (event === "Stop") {
      this.main = this.errorLatched ? MASK.ERROR : MASK.COMPLETED;
    } else if (event === "SubagentStart") {
      this.subagents.set(input.agent_id, MASK.WORKING);
    } else if (event === "SubagentStop") {
      this.subagents.delete(input.agent_id);
    }
    return { control: false, end: event === "SessionEnd" };
  }

  work() {
    this.errorLatched = false;
    this.main = MASK.WORKING;
  }
}

export function discoverHostPid(startPid = process.ppid, readProcess = defaultReadProcess) {
  let pid = startPid;
  for (let depth = 0; depth < 16 && pid > 1; depth++) {
    const current = readProcess(pid);
    if (!current) break;
    if (/(^|\/)(codex|Codex)(\.app\/.*)?$/i.test(current.command) || /ChatGPT\.app\//i.test(current.command)) return pid;
    if (!Number.isSafeInteger(current.ppid) || current.ppid <= 0 || current.ppid === pid) break;
    pid = current.ppid;
  }
  return startPid;
}

function defaultReadProcess(pid) {
  try {
    const line = execFileSync("/bin/ps", ["-o", "ppid=", "-o", "command=", "-p", String(pid)], {
      encoding: "utf8", stdio: ["ignore", "pipe", "ignore"], timeout: 1_000,
    }).trim();
    const match = /^(\d+)\s+(.+)$/.exec(line);
    return match ? { ppid: Number(match[1]), command: match[2].split(/\s+/)[0] } : null;
  } catch {
    return null;
  }
}

function pidIsAlive(pid) {
  try { process.kill(pid, 0); return true; } catch (error) { return error?.code === "EPERM"; }
}

export function request(socketPath, value, timeoutMs = 500) {
  return new Promise((resolve, reject) => {
    if (!socketPath) return reject(new Error("broker unavailable"));
    const socket = net.createConnection(socketPath);
    let data = "";
    const timer = setTimeout(() => socket.destroy(new Error("request timed out")), timeoutMs);
    socket.setEncoding("utf8");
    socket.once("error", reject);
    socket.once("close", () => clearTimeout(timer));
    socket.once("connect", () => socket.write(`${JSON.stringify(value)}\n`));
    socket.on("data", (chunk) => {
      data += chunk;
      const newline = data.indexOf("\n");
      if (newline < 0) return;
      socket.destroy();
      try { resolve(JSON.parse(data.slice(0, newline))); } catch { reject(new Error("invalid response")); }
    });
  });
}

async function removeStaleSocket(socketPath) {
  const entry = await lstat(socketPath).catch((error) => error.code === "ENOENT" ? null : Promise.reject(error));
  if (!entry) return;
  if (!entry.isSocket()) throw new Error("unsafe reporter socket path");
  const listening = await new Promise((resolve, reject) => {
    const socket = net.createConnection(socketPath);
    socket.once("connect", () => { socket.destroy(); resolve(true); });
    socket.once("error", (error) => {
      if (["ECONNREFUSED", "ENOENT"].includes(error.code)) resolve(false);
      else reject(error);
    });
  });
  if (listening) throw new Error("reporter already running");
  await unlink(socketPath);
}

async function prepareLocalDir(localDir) {
  await mkdir(localDir, { recursive: true, mode: 0o700 });
  const entry = await lstat(localDir);
  if (!entry.isDirectory() || (process.getuid && entry.uid !== process.getuid())) {
    throw new Error("unsafe reporter socket directory");
  }
  await chmod(localDir, 0o700);
}

export async function startReporter({ sessionId, cwd, hostPid, env = process.env, now = Date.now,
  intervalMs = REPORT_INTERVAL_MS, isAlive = pidIsAlive, onListening = () => {} }) {
  const root = await workspaceRoot(cwd);
  const groupId = hashId(root);
  const mainMemberId = hashId(sessionId);
  const startedAt = now();
  const localSocket = reporterSocketPath(sessionId);
  const localDir = path.dirname(localSocket);
  const brokerSocket = brokerSocketPath(env);
  const state = new StatusState();
  let closed = false;
  let reporting = Promise.resolve();

  await prepareLocalDir(localDir);
  await removeStaleSocket(localSocket);

  const report = () => reporting = reporting.catch(() => {}).then(async () => {
    const members = [[mainMemberId, state.main], ...[...state.subagents].map(([id, mask]) => [hashId(`${sessionId}:${id}`), mask])];
    let last;
    for (const [memberId, mask] of members) {
      last = await request(brokerSocket, { type: "report", client: "codex", groupId, memberId, pid: process.pid, startedAt, mask });
      if (!last?.ok) throw new Error(last?.error || "broker rejected report");
    }
    return last;
  });

  const removeAll = async () => {
    for (const memberId of [mainMemberId, ...[...state.subagents.keys()].map((id) => hashId(`${sessionId}:${id}`))]) {
      await request(brokerSocket, { type: "remove", client: "codex", groupId, memberId }).catch(() => {});
    }
  };

  const close = async () => {
    if (closed) return;
    closed = true;
    clearInterval(timer);
    await removeAll();
    await new Promise((resolve) => server.close(resolve));
    await unlink(localSocket).catch((error) => { if (error.code !== "ENOENT") throw error; });
  };

  const server = net.createServer((socket) => {
    let data = "";
    let handled = false;
    socket.setEncoding("utf8");
    socket.on("data", async (chunk) => {
      if (handled) return;
      data += chunk;
      if (Buffer.byteLength(data) > MAX_LINE_BYTES) return socket.destroy();
      const newline = data.indexOf("\n");
      if (newline < 0) return;
      handled = true;
      try {
        const input = validateHookInput(JSON.parse(data.slice(0, newline)));
        const stoppedSubagent = input.hook_event_name === "SubagentStop" ? input.agent_id : null;
        const outcome = state.apply(input);
        if (outcome.control) {
          await report();
          const selected = await request(brokerSocket, { type: "select", client: "codex", groupId });
          if (!selected?.ok) throw new Error(selected?.error || "selection rejected");
          socket.end(`${JSON.stringify({ ok: true, revision: selected.revision })}\n`);
        } else {
          if (stoppedSubagent) {
            await request(brokerSocket, {
              type: "remove", client: "codex", groupId,
              memberId: hashId(`${sessionId}:${stoppedSubagent}`),
            }).catch(() => {});
          }
          report().catch(() => {});
          socket.end(`${JSON.stringify({ ok: true })}\n`);
          if (outcome.end) setImmediate(() => void close());
        }
      } catch (error) {
        socket.end(`${JSON.stringify({ ok: false, error: error.message })}\n`);
      }
    });
  });
  await new Promise((resolve, reject) => { server.once("error", reject); server.listen(localSocket, resolve); });
  await chmod(localSocket, 0o600);
  onListening({ localSocket, brokerSocket, groupId, mainMemberId });
  report().catch(() => {});
  const timer = setInterval(() => {
    if (!isAlive(hostPid)) void close();
    else report().catch(() => {});
  }, intervalMs);
  timer.unref?.();
  return { close, groupId, localSocket, mainMemberId, report, state };
}

export function spawnReporter({ sessionId, cwd, hostPid, pluginRoot = process.env.PLUGIN_ROOT }) {
  if (!pluginRoot) throw new Error("PLUGIN_ROOT unavailable");
  const child = spawn(process.execPath, [path.join(pluginRoot, "scripts", "reporter.mjs"), sessionId, cwd, String(hostPid)], {
    detached: true, stdio: "ignore", env: process.env,
  });
  child.unref();
}

async function main() {
  const [sessionId, cwd, rawHostPid] = process.argv.slice(2);
  if (!sessionId || !cwd || !/^\d+$/.test(rawHostPid || "")) process.exit(2);
  await startReporter({ sessionId, cwd, hostPid: Number(rawHostPid) });
  await new Promise(() => {});
}

if (process.argv[1] && pathToFileURL(path.resolve(process.argv[1])).href === import.meta.url) await main();
