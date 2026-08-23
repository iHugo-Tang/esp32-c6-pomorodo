#!/usr/bin/env node

export * from "../plugins/pomodoro-agent-status/scripts/broker.mjs";
import { runBrokerCli } from "../plugins/pomodoro-agent-status/scripts/broker.mjs";
import path from "node:path";
import process from "node:process";
import { pathToFileURL } from "node:url";

if (process.argv[1] && pathToFileURL(path.resolve(process.argv[1])).href === import.meta.url) await runBrokerCli();
