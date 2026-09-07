#!/usr/bin/env node

import { execFileSync } from "node:child_process";
import { chmod, mkdir, readFile, readdir, rename, rm, unlink, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";

export const MINIMUM_OPENCODE_VERSION = "1.18.29";
export const PLUGIN_DIRECTORY_NAME = "pomodoro-agent-status";
const PLUGIN_ID = "pomodoro-agent-status";
const PLUGIN_FILES = ["opencode-server.mjs", "opencode-tui.mjs", "opencode-core.mjs", "reporter.mjs", "broker.mjs"];

const scriptsDir = path.dirname(fileURLToPath(import.meta.url));

export function parseVersion(value) {
  const match = /(\d+)\.(\d+)\.(\d+)/.exec(String(value ?? ""));
  if (!match) throw new Error(`cannot determine an OpenCode version from ${JSON.stringify(value)}`);
  return [Number(match[1]), Number(match[2]), Number(match[3])];
}

export function assertSupportedVersion(version) {
  const minimum = parseVersion(MINIMUM_OPENCODE_VERSION);
  const actual = parseVersion(version);
  const supported = actual[0] > minimum[0] ||
    (actual[0] === minimum[0] && actual[1] > minimum[1]) ||
    (actual[0] === minimum[0] && actual[1] === minimum[1] && actual[2] >= minimum[2]);
  if (!supported) {
    throw new Error(
      `OpenCode ${version} is older than the minimum verified ${MINIMUM_OPENCODE_VERSION}. `
      + "The direct TUI slash registration API cannot be validated on this version; refusing to install.",
    );
  }
  return version;
}

export function resolveConfigDir(explicit = process.env.OPENCODE_CONFIG_DIR) {
  if (explicit) return path.resolve(explicit);
  const home = process.env.HOME || os.homedir?.();
  if (!home) throw new Error("cannot determine the OpenCode configuration directory");
  return path.join(home, ".config", "opencode");
}

async function readJsonConfig(filePath) {
  let contents;
  try {
    contents = await readFile(filePath, "utf8");
  } catch (error) {
    if (error?.code === "ENOENT") return {};
    throw error;
  }
  try {
    return JSON.parse(contents);
  } catch (error) {
    throw new Error(`${path.basename(filePath)} is not valid JSON; refusing to modify it`, { cause: error });
  }
}

async function writeJsonConfig(filePath, value) {
  const temporary = `${filePath}.${process.pid}.${Date.now()}.tmp`;
  try {
    await writeFile(temporary, `${JSON.stringify(value, null, 2)}\n`, { flag: "wx", mode: 0o600 });
    await rename(temporary, filePath);
  } catch (error) {
    await unlink(temporary).catch(() => {});
    throw error;
  }
}

function entryMatches(entry, targetPath, baseDir) {
  if (typeof entry !== "string") return false;
  if (entry.startsWith("file://")) {
    try { return decodeURIComponent(new URL(entry).pathname) === targetPath; } catch { return false; }
  }
  const resolved = path.isAbsolute(entry) ? path.resolve(entry) : path.resolve(baseDir, entry);
  return resolved === targetPath;
}

function pluginEntries(config) {
  const entries = config.plugin;
  if (entries === undefined) return { entries: [], replace: false };
  if (!Array.isArray(entries)) throw new Error("the existing `plugin` value is not an array; refusing to modify the configuration");
  return { entries, replace: true };
}

async function upsertPluginEntry(configPath, targetPath) {
  const baseDir = path.dirname(configPath);
  const config = await readJsonConfig(configPath);
  const { entries, replace } = pluginEntries(config);
  if (entries.some((entry) => entryMatches(entry, targetPath, baseDir))) return false;
  if (!replace) config.plugin = [];
  config.plugin.push(targetPath);
  await writeJsonConfig(configPath, config);
  return true;
}

async function removePluginEntry(configPath, targetPath) {
  const baseDir = path.dirname(configPath);
  const config = await readJsonConfig(configPath);
  const { entries, replace } = pluginEntries(config);
  const remaining = entries.filter((entry) => !entryMatches(entry, targetPath, baseDir));
  if (!replace || remaining.length === entries.length) return false;
  if (remaining.length === 0) delete config.plugin;
  else config.plugin = remaining;
  await writeJsonConfig(configPath, config);
  return true;
}

async function installPluginFiles(targetDir) {
  await mkdir(targetDir, { recursive: true, mode: 0o755 });
  for (const file of PLUGIN_FILES) {
    const source = path.join(scriptsDir, file);
    const target = path.join(targetDir, file);
    const contents = await readFile(source, "utf8");
    let identical = false;
    try {
      identical = (await readFile(target, "utf8")) === contents;
    } catch (error) {
      if (error?.code !== "ENOENT") throw error;
    }
    if (!identical) {
      const temporary = `${target}.${process.pid}.tmp`;
      await writeFile(temporary, contents, { flag: "wx", mode: 0o644 });
      await rename(temporary, target);
    }
  }
  await chmod(targetDir, 0o755);
}

async function assertNotDisabled(configDir) {
  const tuiConfig = await readJsonConfig(path.join(configDir, "tui.json"));
  if (tuiConfig.plugin_enabled?.[PLUGIN_ID] === false) {
    throw new Error(
      `tui.json disables the "${PLUGIN_ID}" plugin via plugin_enabled. `
      + "Remove or enable that entry and restart OpenCode fully before installing.",
    );
  }
}

export async function install({ configDir = resolveConfigDir(), opencodeVersion, pluginDirName = PLUGIN_DIRECTORY_NAME } = {}) {
  assertSupportedVersion(opencodeVersion);
  await assertNotDisabled(configDir);
  const targetDir = path.join(configDir, "plugins", pluginDirName);
  await installPluginFiles(targetDir);
  const serverTarget = path.join(targetDir, "opencode-server.mjs");
  const tuiTarget = path.join(targetDir, "opencode-tui.mjs");
  await upsertPluginEntry(path.join(configDir, "opencode.json"), serverTarget);
  await upsertPluginEntry(path.join(configDir, "tui.json"), tuiTarget);
  return { configDir, pluginDir: targetDir, serverTarget, tuiTarget };
}

export async function uninstall({ configDir = resolveConfigDir(), pluginDirName = PLUGIN_DIRECTORY_NAME } = {}) {
  const targetDir = path.join(configDir, "plugins", pluginDirName);
  const serverTarget = path.join(targetDir, "opencode-server.mjs");
  const tuiTarget = path.join(targetDir, "opencode-tui.mjs");
  await removePluginEntry(path.join(configDir, "opencode.json"), serverTarget).catch((error) => {
    if (error?.code !== "ENOENT") throw error;
  });
  await removePluginEntry(path.join(configDir, "tui.json"), tuiTarget).catch((error) => {
    if (error?.code !== "ENOENT") throw error;
  });
  await rm(targetDir, { recursive: true, force: true });
  return { configDir, removed: targetDir };
}

export async function verify({ configDir = resolveConfigDir(), opencodeVersion, projectDir, pluginDirName = PLUGIN_DIRECTORY_NAME } = {}) {
  assertSupportedVersion(opencodeVersion);
  const targetDir = path.join(configDir, "plugins", pluginDirName);
  for (const file of PLUGIN_FILES) {
    const installed = await readFile(path.join(targetDir, file), "utf8");
    const source = await readFile(path.join(scriptsDir, file), "utf8");
    if (installed !== source) throw new Error(`${file} in ${targetDir} does not match the repository source; rerun install`);
  }
  const serverConfig = await readJsonConfig(path.join(configDir, "opencode.json"));
  const tuiConfig = await readJsonConfig(path.join(configDir, "tui.json"));
  const serverTarget = path.join(targetDir, "opencode-server.mjs");
  const tuiTarget = path.join(targetDir, "opencode-tui.mjs");
  if (!serverConfig.plugin?.some((entry) => entryMatches(entry, serverTarget, configDir))) {
    throw new Error("opencode.json does not activate the Pomodoro server plugin; rerun install and fully restart OpenCode");
  }
  if (!tuiConfig.plugin?.some((entry) => entryMatches(entry, tuiTarget, configDir))) {
    throw new Error("tui.json does not activate the Pomodoro TUI plugin; rerun install and fully restart OpenCode");
  }
  if (tuiConfig.plugin_enabled?.[PLUGIN_ID] === false) {
    throw new Error(`tui.json disables "${PLUGIN_ID}" via plugin_enabled; the TUI surface is not active`);
  }
  if (projectDir) {
    const local = path.join(path.resolve(projectDir), ".opencode", "plugins");
    let names = [];
    try {
      names = await readdir(local);
    } catch (error) {
      if (error?.code !== "ENOENT") throw error;
    }
    if (names.some((name) => name.toLowerCase().includes("pomodoro"))) {
      throw new Error(`${local} contains a project-level Pomodoro plugin copy; remove it to avoid loading the integration twice`);
    }
  }
  return {
    configDir,
    pluginDir: targetDir,
    versionValidated: opencodeVersion,
    runtimeApiCheck: "the TUI plugin validates api.command.register at startup and fails explicitly without it; no Markdown fallback exists",
  };
}

export function installedOpenCodeVersion({ opencode = "opencode", cwd = process.cwd() } = {}) {
  return execFileSync(opencode, ["--version"], { encoding: "utf8", cwd, timeout: 10_000 }).trim();
}

async function main(args = process.argv.slice(2)) {
  const command = args[0];
  const option = (name) => {
    const index = args.indexOf(name);
    return index >= 0 ? args[index + 1] : undefined;
  };
  const configDir = option("--config-dir");
  const projectDir = option("--project-dir");
  const opencodePath = option("--opencode");
  try {
    if (command === "install") {
      const version = option("--version") ?? installedOpenCodeVersion({ opencode: opencodePath ?? "opencode" });
      const result = await install({ configDir, opencodeVersion: version });
      console.log(`installed Pomodoro Agent Status plugin for OpenCode into ${result.pluginDir}`);
      console.log("fully quit every OpenCode process and restart before using /pomodoro-select.");
    } else if (command === "uninstall") {
      const result = await uninstall({ configDir });
      console.log(`removed ${result.removed} and its configuration entries; fully restart OpenCode.`);
    } else if (command === "verify") {
      const version = option("--version") ?? installedOpenCodeVersion({ opencode: opencodePath ?? "opencode" });
      const result = await verify({ configDir, opencodeVersion: version, projectDir });
      console.log(`verified installation in ${result.pluginDir} for OpenCode ${result.versionValidated}`);
      console.log(result.runtimeApiCheck);
    } else {
      console.error("usage: install-opencode.mjs install|uninstall|verify [--config-dir DIR] [--project-dir DIR] [--opencode PATH] [--version X.Y.Z]");
      process.exitCode = 2;
    }
  } catch (error) {
    console.error(`install-opencode: ${error.message}`);
    process.exitCode = 1;
  }
}

if (process.argv[1] && fileURLToPath(import.meta.url) === path.resolve(process.argv[1])) await main();
