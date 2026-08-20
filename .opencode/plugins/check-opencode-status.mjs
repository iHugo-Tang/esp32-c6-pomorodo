// Runnable check for the OpenCode Status plugin's pure aggregation logic.
// Run with: node .opencode/plugins/check-opencode-status.mjs
import assert from "node:assert/strict"
import {
  BIT,
  createAggregator,
  createProtocol,
  createReservation,
  resolveState,
  serializeState,
  serializeAck,
} from "./opencode-status-core.js"

let checks = 0
function ok(cond, msg) {
  assert.ok(cond, msg)
  checks += 1
}

// Event mapping and multi-session aggregation.
const agg = createAggregator()

// Session A works.
let r = agg.handleEvent("session.status", { sessionID: "a", status: { type: "busy" } })
ok(r.revision === 1 && r.mask === BIT.WORKING, "busy -> working")
ok(resolveState(r.mask) === "working", "working resolves to working")

// Session B asks for permission -> waiting outranks working.
r = agg.handleEvent("permission.asked", { sessionID: "b" })
ok(r.revision === 2 && r.mask === BIT.WAITING | BIT.WORKING, "permission -> waiting+working")

// Session A errors -> error outranks waiting.
r = agg.handleEvent("session.error", { sessionID: "a" })
ok(r.revision === 3 && r.mask === BIT.ERROR | BIT.WAITING, "error outranks waiting")

// Session A resumes working -> error is cleared by new work.
r = agg.handleEvent("session.status", { sessionID: "a", status: { type: "retry" } })
ok(r.revision === 4 && r.mask === BIT.WAITING | BIT.WORKING, "new work clears error")

// Session B reply resolves waiting; working remains.
r = agg.handleEvent("permission.replied", { sessionID: "b" })
ok(r.revision === 5 && r.mask === BIT.WORKING, "reply clears waiting")

// Session A idles -> completed.
r = agg.handleEvent("session.idle", { sessionID: "a" })
ok(r.revision === 6 && r.mask === BIT.COMPLETED, "idle -> completed")

// Heartbeat keeps revision stable.
const heartbeat = agg.snapshot()
ok(heartbeat.revision === 6 && heartbeat.mask === BIT.COMPLETED, "heartbeat stable")

// ACK of completed clears completed and reveals idle without a revision change.
const ack = agg.handleAck(6, BIT.COMPLETED)
ok(ack.revision === 6 && ack.mask === 0, "ack clears completed without revision bump")

// A new error is a new revision and is not cleared by the old ack.
r = agg.handleEvent("session.error", { sessionID: "b" })
ok(r.revision === 7 && r.mask === BIT.ERROR, "new error after ack is new revision")

// Stale ack (wrong revision) is ignored.
ok(agg.handleAck(6, BIT.ERROR) === null, "stale ack ignored")

// Session deletion removes it from the aggregate; an ACKed session
// contributes nothing, so the mask and revision are unchanged.
r = agg.handleEvent("session.deleted", { sessionID: "a" })
ok(r.revision === 7 && r.mask === BIT.ERROR, "deleted session removed")

// Question asked maps to waiting too.
r = agg.handleEvent("question.asked", { sessionID: "c" })
ok(r.revision === 8 && r.mask === BIT.ERROR | BIT.WAITING, "question.asked -> waiting")

// Question replies and rejections clear waiting without inventing completion.
r = agg.handleEvent("question.replied", { sessionID: "c" })
ok(r.revision === 9 && r.mask === BIT.ERROR, "question.replied clears waiting")
r = agg.handleEvent("question.asked", { sessionID: "c" })
r = agg.handleEvent("question.rejected", { sessionID: "c" })
ok(r.revision === 11 && r.mask === BIT.ERROR, "question.rejected clears waiting")

// Permission rejection has the same release behavior.
r = agg.handleEvent("permission.asked", { sessionID: "c" })
r = agg.handleEvent("permission.rejected", { sessionID: "c" })
ok(r.revision === 13 && r.mask === BIT.ERROR, "permission.rejected clears waiting")

// Disabled transport suppresses writes but keeps the current aggregate.
const protocol = createProtocol()
ok(protocol.handleEvent("session.status", {
  sessionID: "d", status: { type: "busy" },
}) === null, "disabled transport suppresses event writes")
const enabledSnapshot = protocol.setEnabled(true)
ok(enabledSnapshot === "OC1 STATE 1 4", "enable sends the current snapshot")
ok(protocol.heartbeat() === enabledSnapshot, "heartbeat keeps revision stable")
ok(protocol.setEnabled(false) === null, "disable suppresses state writes")
protocol.handleEvent("session.idle", { sessionID: "d" })
ok(protocol.setEnabled(true) === "OC1 STATE 2 8",
  "re-enable sends changes collected while disabled")

// Serialization format matches the device contract.
ok(serializeState(14, 5) === "OC1 STATE 14 5", "state serialization")
ok(serializeAck(14, 1) === "OC1 ACK 14 1", "ack serialization")

// Cooperative USB port reservation transitions.
const res = createReservation()
ok(res.held() === false, "reservation starts released")
let rt = res.update(false)
ok(rt.held === false && !rt.acquired && !rt.released, "no-op while released")
rt = res.update(true, true)
ok(rt.held && rt.acquired && !rt.released, "acquired when marker appears")
ok(res.held() === true, "held after acquire")
rt = res.update(true, true)
ok(rt.held && !rt.acquired && !rt.released, "still held, no edge")
rt = res.update(true, false)
ok(!rt.held && rt.released, "stale marker PID releases the port")
rt = res.update(true, true)
ok(rt.acquired, "fresh marker re-acquires")
rt = res.update(false)
ok(rt.released && !rt.held, "released when marker is removed")
rt = res.update(false)
ok(!rt.acquired && !rt.released && !rt.held, "no-op after release")

console.log(`opencode-status check: ${checks} checks passed`)
