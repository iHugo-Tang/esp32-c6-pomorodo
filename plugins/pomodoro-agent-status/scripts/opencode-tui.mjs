#!/usr/bin/env node

import process from "node:process";
import {
  ActivationSession, brokerSocketPath, failureFeedback, selectionFeedback, workspaceRoot,
} from "./opencode-core.mjs";

const COMMAND_SLUG = "pomodoro-select";

function createTuiPlugin(options = {}) {
  let active = null;

  const selectCurrentWorkspace = async (api) => {
    const previous = active;
    active = null;
    previous?.stop();
    try {
      const path = api?.state?.path ?? {};
      const root = await workspaceRoot(path.worktree ?? path.directory ?? process.cwd());
      const session = new ActivationSession({
        root,
        env: options.env ?? process.env,
        socketPath: options.socketPath ?? brokerSocketPath(options.env ?? process.env),
      });
      active = session;
      const result = await session.activate();
      api?.ui?.toast?.(selectionFeedback(result));
    } catch (error) {
      active?.stop();
      active = null;
      api?.ui?.toast?.(failureFeedback(error));
    }
  };

  return {
    stop() {
      active?.stop();
      active = null;
    },
    register(api) {
      api.command.register(() => [{
        title: "Pomodoro: select workspace",
        value: "pomodoro-agent-status.select",
        description: "Select the current OpenCode Workspace as the Pomodoro Agent Source",
        slash: { name: COMMAND_SLUG },
        onSelect: () => { void selectCurrentWorkspace(api); },
      }]);
    },
  };
}

function assertDirectCommandApi(api) {
  const command = api?.command;
  if (typeof command?.register !== "function" || typeof command?.trigger !== "function") {
    throw new Error(
      "pomodoro-agent-status: the direct TUI slash registration API (api.command.register) is unavailable in this OpenCode version. "
      + "Installation stopped without registering a Markdown prompt command or any model-backed fallback.",
    );
  }
  if (typeof api?.ui?.toast !== "function") {
    throw new Error("pomodoro-agent-status: the TUI toast API is unavailable; no model-backed fallback is registered.");
  }
}

export default {
  id: "pomodoro-agent-status",
  tui: async (api, options = {}) => {
    assertDirectCommandApi(api);
    const plugin = createTuiPlugin(options);
    plugin.register(api);
    api.lifecycle?.onDispose?.(() => { plugin.stop(); });
  },
};
