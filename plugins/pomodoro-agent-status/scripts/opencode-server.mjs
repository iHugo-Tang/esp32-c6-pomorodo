#!/usr/bin/env node

import process from "node:process";
import {
  REPORT_INTERVAL_MS, MemberState, brokerSocketPath, eventSessionId, hashId, memberId, reduceMemberEvent, request, workspaceRoot,
} from "./opencode-core.mjs";

const IGNORED_EVENTS = new Set(["session.compacted", "message.removed", "message.part.removed", "message.part.delta"]);

export const PomodoroAgentStatusPlugin = async (input, options = {}) => {
  const env = options.env ?? process.env;
  const socketPath = options.socketPath ?? brokerSocketPath(env);
  const baseRoot = options.baseRoot ?? await workspaceRoot(input?.worktree ?? input?.directory ?? process.cwd());
  const pid = options.pid ?? process.pid;
  const startedAt = options.startedAt ?? Date.now();
  const sessions = new Map();
  const roots = new Map();
  let reporting = Promise.resolve();
  let handling = Promise.resolve();
  let disposed = false;

  const groupIdFor = async (directory) => {
    if (!directory) return hashId(baseRoot);
    if (roots.has(directory)) return roots.get(directory);
    const root = await workspaceRoot(directory).catch(() => baseRoot);
    const groupId = hashId(root);
    roots.set(directory, groupId);
    return groupId;
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

  const handleEvent = async (event) => {
    try {
      if (!event?.type || IGNORED_EVENTS.has(event.type)) return;
      const properties = event.properties ?? {};
      if (event.type === "session.created" || event.type === "session.updated") {
        const id = properties.info?.id;
        if (!id) return;
        const group = await groupIdFor(properties.info.directory);
        const existing = sessions.get(id);
        if (existing && existing.groupId !== group) {
          // Membership moves atomically: drop the old source's member before any
          // further report addresses the new Workspace.
          removeMember(existing.groupId, memberId(id));
          existing.groupId = group;
        }
        if (!existing) sessions.set(id, { groupId: group, state: new MemberState() });
        reportMember(sessions.get(id).groupId, memberId(id), sessions.get(id).state.mask);
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
  };

  return {
    dispose,
    event: ({ event }) => {
      // Serialize membership mutations so concurrent events cannot interleave
      // a group move with a report for the stale group.
      const run = handling.then(() => handleEvent(event));
      handling = run.catch(() => {});
      return run;
    },
  };
};
