// Pure OpenCode Status aggregation core. No hardware or OpenCode runtime
// dependencies, so it can be unit-tested with plain Node/Bun.
//
// Bit assignments: ERROR=1, WAITING=2, WORKING=4, COMPLETED=8. Priority is
// Error > Waiting > Working > Completed > Idle. A zero mask means Idle.

export const BIT = { ERROR: 1, WAITING: 2, WORKING: 4, COMPLETED: 8 }

export function createAggregator() {
  const state = {
    sessions: new Map(),
    revision: 0,
    mask: 0,
    sentRevision: null,
    sentMask: null,
  }

  function ensureSession(id) {
    if (!state.sessions.has(id)) {
      state.sessions.set(id, {
        bits: 0,
        errorAcked: false,
        completedAcked: false,
      })
    }
    return state.sessions.get(id)
  }

  function computeMask() {
    let mask = 0
    for (const session of state.sessions.values()) {
      if ((session.bits & BIT.ERROR) !== 0 && !session.errorAcked) mask |= BIT.ERROR
      if ((session.bits & BIT.WAITING) !== 0) mask |= BIT.WAITING
      if ((session.bits & BIT.WORKING) !== 0) mask |= BIT.WORKING
      if ((session.bits & BIT.COMPLETED) !== 0 && !session.completedAcked) {
        mask |= BIT.COMPLETED
      }
    }
    return mask
  }

  function push(reason) {
    const next = computeMask()
    const changed = next !== state.sentMask
    if (changed) {
      if (reason === "event") state.revision += 1
      state.sentMask = next
    }
    return { changed, revision: state.revision, mask: state.sentMask }
  }

  function handleEvent(type, props = {}) {
    const sessionID = props.sessionID ?? props.info?.id ?? null
    if (!sessionID) return null
    const session = ensureSession(sessionID)

    if (type === "session.status") {
      const status = props.status ?? props
      const kind = typeof status === "string" ? status : status.type
      if (kind === "busy" || kind === "retry") {
        session.bits &= ~BIT.ERROR
        session.errorAcked = false
        session.bits |= BIT.WORKING
        session.completedAcked = false
      }
    } else if (type === "session.idle") {
      session.bits &= ~BIT.WORKING
      session.bits |= BIT.COMPLETED
      session.completedAcked = false
    } else if (type === "session.error") {
      session.bits |= BIT.ERROR
      session.errorAcked = false
    } else if (type === "session.deleted") {
      state.sessions.delete(sessionID)
    } else if (type === "permission.asked" || type === "question.asked") {
      session.bits |= BIT.WAITING
    } else if (type === "permission.replied" ||
               type === "permission.rejected" ||
               type === "question.replied" ||
               type === "question.rejected" ||
               type === "question.answered") {
      session.bits &= ~BIT.WAITING
      if ((session.bits & BIT.WORKING) !== 0) session.completedAcked = false
    }

    return push("event")
  }

  function handleAck(revision, mask) {
    if (revision !== state.revision) return null
    if ((mask & BIT.ERROR) !== 0) {
      for (const session of state.sessions.values()) {
        session.errorAcked = true
      }
    }
    if ((mask & BIT.COMPLETED) !== 0) {
      for (const session of state.sessions.values()) {
        session.completedAcked = true
      }
    }
    return push("ack")
  }

  function snapshot() {
    return { revision: state.revision, mask: state.sentMask ?? computeMask() }
  }

  return { handleEvent, handleAck, snapshot }
}

export function createProtocol() {
  const aggregator = createAggregator()
  let enabled = false

  return {
    handleEvent(type, properties) {
      const result = aggregator.handleEvent(type, properties)
      return enabled && result?.changed
        ? serializeState(result.revision, result.mask)
        : null
    },
    handleAck(revision, mask) {
      const result = aggregator.handleAck(revision, mask)
      return enabled && result?.changed
        ? serializeState(result.revision, result.mask)
        : null
    },
    setEnabled(value) {
      enabled = value
      if (!enabled) return null
      const snapshot = aggregator.snapshot()
      return serializeState(snapshot.revision, snapshot.mask)
    },
    heartbeat() {
      if (!enabled) return null
      const snapshot = aggregator.snapshot()
      return serializeState(snapshot.revision, snapshot.mask)
    },
  }
}

// Resolves a mask to a display state following the documented priority.
export function resolveState(mask) {
  if (mask & BIT.ERROR) return "error"
  if (mask & BIT.WAITING) return "waiting"
  if (mask & BIT.WORKING) return "working"
  if (mask & BIT.COMPLETED) return "completed"
  return "idle"
}

export function serializeState(revision, mask) {
  return `OC1 STATE ${revision} ${mask}`
}

export function serializeAck(revision, mask) {
  return `OC1 ACK ${revision} ${mask}`
}
