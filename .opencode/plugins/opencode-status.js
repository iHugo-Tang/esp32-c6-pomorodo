// Project plugin that mirrors this project's OpenCode sessions to the Pomodoro
// device over the USB Serial/JTAG link. No npm dependencies; uses only Node
// built-ins and macOS `/dev/cu.usbmodem*` device files.

import { spawn, spawnSync } from "node:child_process"
import fs from "node:fs"
import { createProtocol } from "./opencode-status-core.js"

const HEARTBEAT_MS = 3000
const RECONNECT_MS = 2000

export const OpenCodeStatusPlugin = async ({ project, client, directory }) => {
  const log = (message) => {
    try {
      client.app.log({ body: { service: "opencode-status", level: "debug", message } })
    } catch {
      // Logging is best-effort and must never take OpenCode down.
    }
  }

  const protocol = createProtocol()
  let port = null
  let outFd = -1
  let reader = null
  let heartbeat = null
  let reconnectTimer = null
  let buffer = ""

  function close() {
    clearInterval(heartbeat)
    heartbeat = null
    if (reader) {
      reader.kill()
      reader = null
    }
    if (outFd >= 0) {
      try { fs.closeSync(outFd) } catch { /* already closed */ }
      outFd = -1
    }
    port = null
    protocol.setEnabled(false)
  }

  function send(line) {
    if (outFd < 0 || line === null) return
    try {
      fs.writeSync(outFd, line + "\n")
    } catch {
      scheduleReconnect()
    }
  }

  function sendSnapshot() {
    send(protocol.heartbeat())
  }

  function onData(chunk) {
    buffer += chunk
    let newline = buffer.indexOf("\n")
    while (newline >= 0) {
      const line = buffer.slice(0, newline).replace(/\r$/, "")
      buffer = buffer.slice(newline + 1)
      if (line.startsWith("OC1 ENABLED")) {
        send(protocol.setEnabled(true))
        startHeartbeat()
      } else if (line.startsWith("OC1 DISABLED")) {
        protocol.setEnabled(false)
        clearInterval(heartbeat)
        heartbeat = null
      } else if (line.startsWith("OC1 ACK ")) {
        const parts = line.split(" ")
        const revision = Number(parts[2])
        const mask = Number(parts[3])
        if (Number.isInteger(revision) && Number.isInteger(mask)) {
          send(protocol.handleAck(revision, mask))
        }
      }
      newline = buffer.indexOf("\n")
    }
  }

  function startHeartbeat() {
    if (!heartbeat) {
      heartbeat = setInterval(sendSnapshot, HEARTBEAT_MS)
    }
  }

  function scheduleReconnect() {
    close()
    if (!reconnectTimer) {
      reconnectTimer = setTimeout(connect, RECONNECT_MS)
    }
  }

  function resolvePort() {
    const forced = process.env.POMODORO_PORT
    if (forced) return fs.existsSync(forced) ? forced : null
    let candidates = []
    try {
      candidates = fs
        .readdirSync("/dev")
        .filter((name) => name.startsWith("cu.usbmodem"))
        .map((name) => `/dev/${name}`)
    } catch {
      return null
    }
    if (candidates.length === 1) return candidates[0]
    // Multiple candidates: never guess; require POMODORO_PORT.
    return null
  }

  function connect() {
    reconnectTimer = null
    if (port) return
    const candidate = resolvePort()
    if (!candidate) {
      scheduleReconnect()
      return
    }
    try {
      spawnSync("stty", ["-f", candidate, "raw", "-echo"], { stdio: "ignore" })
      outFd = fs.openSync(candidate, "w")
      reader = spawn("cat", [candidate], {
        stdio: ["ignore", "pipe", "inherit"],
      })
      reader.stdout.setEncoding("utf8")
      reader.stdout.on("data", onData)
      reader.on("exit", () => {
        reader = null
        scheduleReconnect()
      })
      port = candidate
      log(`Connected to ${candidate}`)
    } catch (e) {
      log(`Serial connect failed: ${e.message}`)
      scheduleReconnect()
    }
  }

  function dispatch(type, properties = {}) {
    try {
      send(protocol.handleEvent(type, properties))
    } catch {
      // A plugin bug must never fail an OpenCode task.
    }
  }

  connect()

  return {
    event: async ({ event }) => {
      const raw = event?.payload ?? event
      const type = raw.type ?? event?.type
      const props = raw.properties ?? raw
      const eventDirectory = raw.directory ?? props.directory ?? directory
      if (eventDirectory && eventDirectory !== directory) return
      dispatch(type, props)
    },
  }
}
