#!/usr/bin/env node

import process from "node:process";
import {
  MASK, REPORT_INTERVAL_MS, MemberState, brokerSocketPath, eventSessionId, hashId, memberId, reduceMemberEvent, request, workspaceRoot,
} from "./opencode-core.mjs";

const IGNORED_EVENTS = new Set(["session.compacted", "message.removed", "message.part.removed", "message.part.delta"]);

function eventProperties(event) {
  return event?.properties ?? {};
}

export const PomodoroAgentStatusPlugin = async (input, options = {}) => {
  const env = options.env ?? process.env;
  const socketPath = options.socketPath ?? brokerSocketPath(env);
  const baseRoot = options.baseRoot ?? await workspaceRoot(input?.worktree ?? input?.directory ?? process.cwd());
  const pid = options.pid ?? process.pid;
  const startedAt = options.startedAt ?? Date.now();
  const sessions = new Map();
  const roots = new Map();
  let reporting = Promise.resolve();
  let disposed = false;

  const groupIdFor = async (directory) => {
    if (!directory) return { groupId: hashId(baseRoot), root: baseRoot };
    if (roots.has(directory)) return roots.get(directory);
    const root = await workspaceRoot(directory).catch(() => baseRoot);
    const entry = { groupId: hashId(root), root };
    roots.set(directory, entry);
    return entry;
  };

  const reportMember = (group, id, mask) => {
    reporting = reporting.catch(() => {}).then(async () => {
      await request(socketPath, {
        type: "report", groupId: group, memberId: id, pid, startedAt, mask, host: "opencode",
      }).catch(() => {});
    });
  };

  const removeMember = (group, id) => {
    reporting = reporting.catch(() => {}).then(async () => {
      await request(socketPath, { type: "remove", groupId: group, memberId: id, host: "opencode" }).catch(() => {});
    });
  };

  const timer = setInterval(() => {
    if (disposed) return;
    for (const [id, member] of sessions) reportMember(member.groupId, id, member.state.mask);
  }, REPORT_INTERVAL_MS);
  timer.unref?.();

  const dispose = async () => {
    disposed = true;
    clearInterval(timer);
    await reporting.catch(() => {});
  };

  return {
    dispose,
    event: async ({ event }) => {
      try {
        if (!event?.type || IGNORED_EVENTS.has(event.type)) return;
        const properties = eventProperties(event);
        if (event.type === "session.created" || event.type === "session.updated") {
          const id = properties.info?.id;
          if (!id) return;
          const existing = sessions.get(id);
          const group = await groupIdFor(properties.info.directory);
          if (existing && existing.groupId !== group.groupId) removeMember(existing.groupId, memberId(id));
          if (!existing) sessions.set(id, { groupId: group.groupId, state: new MemberState() });
          reportMember(group.groupId, memberId(id), sessions.get(id).state.mask);
          return;
        }
        const id = eventSessionId(event);
        if (!id) return;
        if (event.type === "session.deleted") {
          const member = sessions.get(id);
          sessions.delete(id);
          if (member) removeMember(member.groupId, memberId(id));
          return;
        }
        let member = sessions.get(id);
        if (!member) {
          // Sessions resumed after an OpenCode restart never replay session.created;
          // attribute them to this process's Workspace until session.updated corrects it.
          member = { groupId: hashId(baseRoot), state: new MemberState() };
          sessions.set(id, member);
        }
        const before = member.state.mask;
        reduceMemberEvent(member.state, event);
        if (member.state.mask !== before) reportMember(member.groupId, memberId(id), member.state.mask);
      } catch {
        // Lifecycle reporting is advisory and must never interrupt OpenCode.
      }
    },
  };
};
