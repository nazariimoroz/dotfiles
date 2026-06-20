-- Machine-local Hyprland overrides. GITIGNORED — not shared across machines,
-- because device names below come from `hyprctl devices` and are specific to
-- this hardware. Copy from user_config.lua.sample on a new machine and fill in
-- the names you get from `hyprctl devices -j | jq -r '.mice[].name'`.

return {
	-- Per-device pointer speed. sensitivity: -1.0 (slow) .. 1.0 (fast),
	-- 0.0 = libinput default. Each entry is passed straight to hl.device{}.
	devices = {
		{ name = "asuf1209:00-2808:0219-touchpad", sensitivity = 0.13 },
		{ name = "asuf1209:00-2808:0219-mouse", sensitivity = 0.4 },
	},
}
