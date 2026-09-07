#!/usr/bin/env node

import net from "node:net";
import { MASK, REPORT_INTERVAL_MS, brokerSocketPath, ensureBroker, hashId, request, workspaceRoot } from "./reporter.mjs";

export { MASK, REPORT_INTERVAL_MS, brokerSocketPath, ensureBroker, hashId, request, workspaceRoot };

const SELECT_TIMEOUT_MS = 2_000;
const TUI_MEMBER_PREFIX = "pomodoro-tui";

export class MemberState {
  constructor() {
    this.phase = "idle";
    this.errorLatched = false;
  }

  work({ clearsError = true } = {}) {
    if (clearsError) this.errorLatched = false;
    this.phase = "working";
  }

  wait() {
    this.phase = "waiting";
  }

  resume() {
    this.phase = "working";
  }

  fail() {
    this.errorLatched = true;
    this.phase = "idle";
  }

  complete() {
    if (!this.errorLatched) this.phase = "completed";
  }

  get mask() {
    if (this.errorLatched) return MASK.ERROR;
    if (this.phase === "working") return MASK.WORKING;
    if (this.phase === "waiting") return MASK.WAITING;
    if (this.phase === "completed") return MASK.COMPLETED;
    return MASK.IDLE;
  }
}

export function memberId(sessionId) {
  return hashId(`opencode:${sessionId}`);
}

export function tuiMemberId(root) {
  return hashId(`${TUI_MEMBER_PREFIX}:${root}`);
}

export function eventSessionId(event) {
  const properties = event?.properties ?? {};
  return properties.info?.id ?? properties.sessionID ?? properties.part?.sessionID ?? null;
}

export function reduceMemberEvent(state, event) {
  const type = event?.type;
  const properties = event?.properties ?? {};
  if (type === "session.status") {
    const status = properties.status?.type;
    if (status === "busy") state.work();
    else if (status === "retry") state.work({ clearsError: false });
    else if (status === "idle") state.complete();
  } else if (type === "session.idle") {
    state.complete();
  } else if (type === "session.error") {
    if (properties.sessionID) state.fail();
  } else if (type === "permission.asked") {
    state.wait();
  } else if (type === "permission.replied") {
    state.resume();
  } else if (type === "question.asked") {
    state.wait();
  } else if (type === "question.replied" || type === "question.rejected") {
    state.resume();
  } else if (type === "message.updated") {
    if (properties.info?.role === "user") state.work();
  } else if (type === "message.part.updated") {
    const part = properties.part;
    if (part?.type === "tool") {
      if (part.state?.status === "error") state.fail();
      else if (part.state?.status === "pending" || part.state?.status === "running") state.work();
    }
  }
}

function hasListener(socketPath) {
  return new Promise((resolve, reject) => {
    if (!socketPath) return resolve(false);
    const socket = net.createConnection(socketPath);
    socket.once("connect", () => { socket.destroy(); resolve(true); });
    socket.once("error", (error) => {
      if (["ECONNREFUSED", "ENOENT", "ENOTSOCK"].includes(error.code)) resolve(false);
      else reject(error);
    });
  });
}

export function failureCategory(error, reply) {
  if (reply && reply.ok === false) return "broker rejected the request";
  const code = error?.code;
  const message = error?.message ?? String(error ?? "unknown failure");
  if (["ECONNREFUSED", "ENOENT", "ENOTSOCK", "EACCES", "EPERM"].includes(code)) return "broker unavailable";
  if (/broker unavailable|broker did not start|timed out|invalid response|request timed out|unsafe|unavailable|not a socket/i.test(message)) {
    return "broker unavailable";
  }
  return "status source unavailable";
}

export class ActivationSession {
  constructor({ root, env = process.env, socketPath = brokerSocketPath(env), pid = process.pid, startedAt = Date.now() } = {}) {
    if (!root) throw new Error("workspace root is required");
    this.root = root;
    this.env = env;
    this.socketPath = socketPath;
    this.groupId = hashId(root);
    this.memberId = tuiMemberId(root);
    this.pid = pid;
    this.startedAt = startedAt;
    this.timer = null;
    this.reporting = Promise.resolve();
    this.stopped = false;
  }

  async report(mask = MASK.IDLE) {
    const reply = await request(this.socketPath, {
      type: "report", groupId: this.groupId, memberId: this.memberId,
      pid: this.pid, startedAt: this.startedAt, mask, host: "opencode",
    }, SELECT_TIMEOUT_MS);
    if (!reply?.ok) throw new Error(reply?.error || "broker rejected report");
    return reply;
  }

  keepAlive() {
    if (this.timer || this.stopped) return;
    this.timer = setInterval(() => {
      this.reporting = this.reporting.catch(() => {}).then(() => this.report()).catch(() => {});
    }, REPORT_INTERVAL_MS);
    this.timer.unref?.();
  }

  stop() {
    this.stopped = true;
    if (this.timer) {
      clearInterval(this.timer);
      this.timer = null;
    }
  }

  async activate() {
    const brokerWasRunning = await hasListener(this.socketPath);
    await ensureBroker(this.socketPath, { env: this.env });
    await this.report();
    const reply = await request(this.socketPath, {
      type: "select", groupId: this.groupId, host: "opencode",
    }, SELECT_TIMEOUT_MS);
    if (!reply?.ok) throw new Error(reply?.error || "broker rejected selection");
    this.keepAlive();
    return {
      outcome: brokerWasRunning ? "selected" : "waiting",
      revision: reply.revision,
      groupId: this.groupId,
      memberId: this.memberId,
    };
  }
}

export function selectionFeedback(result) {
  if (result.outcome === "waiting") {
    return {
      variant: "info",
      message: `Pomodoro: selection saved (revision ${result.revision}); waiting for the broker to take over the device.`,
    };
  }
  return {
    variant: "success",
    message: `Pomodoro: selected this OpenCode Workspace (revision ${result.revision}).`,
  };
}

export function failureFeedback(error) {
  return {
    variant: "error",
    message: `Pomodoro: selection failed (${failureCategory(error)}); the previous selection is unchanged.`,
  };
}
