#!/usr/bin/env node

import process from "node:process";
// @opencode/plugin/tui only exists inside the OpenCode host; under plain Node
// (check.mjs) Plugin.define is the identity function anyway.
let define;
try {
  ({ Plugin: { define } } = await import("@opencode/plugin/tui"));
} catch {
  define = (plugin) => plugin;
}
import {
  ActivationSession, brokerSocketPath, failureFeedback, selectionFeedback, workspaceRoot,
} from "./opencode-core.mjs";

const COMMAND_SLUG = "pomodoro-select";

export default define({
  id: "pomodoro-agent-status.cli",
  setup(context) {
    let active = null;

    const selectCurrentWorkspace = async () => {
      let candidate = null;
      try {
        const location = context.location ?? context.data?.location?.default?.() ?? {};
        const root = await workspaceRoot(location.directory ?? process.cwd());
        candidate = new ActivationSession({
          root,
          env: context.options?.env ?? process.env,
          socketPath: context.options?.socketPath ?? brokerSocketPath(context.options?.env ?? process.env),
        });
        const result = await candidate.activate();
        const previous = active;
        active = candidate;
        // Retain the previous activation until the replacement has succeeded so a
        // failed selection cannot starve the still-selected Workspace's keep-alive.
        previous?.stop();
        context.ui.toast.show(selectionFeedback(result));
      } catch (error) {
        candidate?.stop();
        context.ui.toast.show(failureFeedback(error));
      }
    };

    // keymap.layer is a reactive primitive: it must run inside a Solid owner,
    // so commands are registered from an app-level slot render instead of
    // directly in setup().
    const disposeSlot = context.ui.slot({
      append: "app",
      render: () => {
        context.keymap.layer(() => ({
          mode: "global",
          commands: [{
            id: "pomodoro-agent-status.select",
            title: "Pomodoro: select workspace",
            slash: { name: COMMAND_SLUG },
            run: () => { void selectCurrentWorkspace(); },
          }],
        }));
        return null;
      },
    });

    return () => {
      disposeSlot?.();
      active?.stop();
      active = null;
    };
  },
});
