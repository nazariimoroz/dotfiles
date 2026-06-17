# Dotfiles (`~/.config`)

Personal config repo. Notable tools live in their own dirs: `hypr/`, `waybar/`,
`wofi/`, `easyhub/` (a custom FTXUI TUI control hub).

## Hyprland uses the new native Lua config (0.55+)

Hyprland **0.55** introduced a first-class **Lua configuration** that replaces
the old hyprlang (`hyprland.conf`) syntax. This machine uses it:
`~/.config/hypr/hyprland.lua`.

Key facts when editing the Hyprland config here:

- **Loading**: at startup Hyprland checks for `hyprland.lua`; if present it loads
  that **instead of** `hyprland.conf`. The check happens once at init —
  switching config type (or first creating `hyprland.lua`) requires a full
  Hyprland restart, not just a reload.
- **Global namespace is `hl`**. Do not write raw `hyprland.conf` directives;
  express everything through the `hl` Lua API. Functions actually used in
  `hypr/hyprland.lua` (use these as the reference for available API):
  - `hl.monitor{...}` — monitor setup
  - `hl.config{ general=…, decoration=…, input=…, animations=… }` — settings blocks
  - `hl.env(name, value)` — environment variables
  - `hl.on(event, fn)` — event callbacks, e.g. `hl.on("hyprland.start", fn)`,
    `hl.on("window.open", fn)`, `hl.on("window.move_to_workspace", fn)`
  - `hl.exec_cmd(cmd)` — run a command (used for autostart inside the
    `hyprland.start` callback)
  - `hl.bind(keys, dispatcher, opts?)` — keybindings; dispatchers come from
    `hl.dsp.*` (e.g. `hl.dsp.exec_cmd`, `hl.dsp.window.close()`,
    `hl.dsp.focus{...}`, `hl.dsp.workspace.toggle_special(...)`)
  - `hl.dispatch(dispatcher)` — fire a dispatcher imperatively
  - `hl.curve(name, {...})` / `hl.animation{ leaf=…, bezier=… }` — bezier curves
    and per-leaf animations (incl. `layers`, `layersIn`, `layersOut`)
  - `hl.device{...}`, `hl.gesture{...}`
  - `hl.window_rule{...}`, `hl.workspace_rule{...}`
- **Lua-only powers**: timers, events, callbacks, layout data, user-defined
  layouts (Layout API) — things that previously needed plugins.
- **hyprlang deprecation**: legacy syntax stays for ~1–2 releases after 0.55,
  then dropped. No new config features are added to hyprlang anymore.
- For anything not already demonstrated in `hyprland.lua` (e.g. layer rules),
  **consult the official docs** rather than guessing — the Lua API surface is
  new and evolving:
  - Wiki: https://wiki.hypr.land/
  - Announcement: https://hypr.land/news/26_lua/
