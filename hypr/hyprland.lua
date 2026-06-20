local adebug = require("adebug")

------------------
---- MONITORS ----
------------------

hl.monitor({ output = "", mode = "preferred", position = "auto", scale = "auto" })
hl.monitor({ output = "eDP-1", mode = "2560x1600@165", position = "0x0", scale = 1.67 })
hl.monitor({ output = "DP-2", mode = "1920x1080@60", position = "1280x0", scale = 1 })

---------------------
---- MY PROGRAMS ----
---------------------

local terminal = "kitty"
local menu = os.getenv("HOME") .. "/.config/wofi/toggle.sh"
local bar = "waybar"
local wallpaper = "hyprpaper"

-- Floating layout for the win+M "magic" special-workspace dashboard. easyhub
-- keeps a fixed-width column on the right; the other panes (btop) float to fill
-- the remaining space (instead of dwindle splitting them 50/50). Widths are
-- derived from the monitor so the fill always reaches up to easyhub.
local MAGIC_EASYHUB_W = 510 -- right pane width (logical px)
local MAGIC_INSET = 12 -- left inset (outer gap + border)
local MAGIC_TOP = 31 -- top inset (clears the waybar + gap)
local MAGIC_GAP = 14 -- gap between panes
local MAGIC_RIGHT_GAP = 9 -- gap to the right edge
local MAGIC_BOTTOM_GAP = 10 -- gap to the bottom edge

local function float_window_to(w, x, y, ww, h)
	hl.dispatch(hl.dsp.window.float({ action = "enable", window = w }))
	hl.dispatch(hl.dsp.window.resize({ x = ww, y = h, exact = true, window = w }))
	hl.dispatch(hl.dsp.window.move({ x = x, y = y, exact = true, window = w }))
end

-- All windows currently on the "magic" special workspace.
local function magic_windows()
	local out = {}
	for _, w in ipairs(hl.get_windows()) do
		if w.workspace and w.workspace.name == "special:magic" then
			out[#out + 1] = w
		end
	end
	return out
end

-- Pin the window titled `fixed_title` to a fixed-width column hugging `side`
-- ("left"/"right"), then lay the remaining magic windows out to fill the rest
-- of the space, split evenly side by side.
local function align_fixed_pane(fixed_title, fixed_w, side)
	local fixed, rest = nil, {}
	for _, w in ipairs(magic_windows()) do
		if w.title == fixed_title then
			fixed = w
		else
			rest[#rest + 1] = w
		end
	end
	if not fixed or not fixed.monitor then
		return
	end

	local mon = fixed.monitor
	local mw = math.floor(mon.width / mon.scale)
	local mh = math.floor(mon.height / mon.scale)
	local h = mh - MAGIC_TOP - MAGIC_BOTTOM_GAP

	local fixed_x, rest_x0, rest_w
	if side == "right" then
		fixed_x = mw - MAGIC_RIGHT_GAP - fixed_w
		rest_x0 = MAGIC_INSET
		rest_w = fixed_x - MAGIC_GAP - rest_x0
	else -- "left"
		fixed_x = MAGIC_INSET
		rest_x0 = fixed_x + fixed_w + MAGIC_GAP
		rest_w = (mw - MAGIC_RIGHT_GAP) - rest_x0
	end
	float_window_to(fixed, fixed_x, MAGIC_TOP, fixed_w, h)

	local n = #rest
	if n == 0 then
		return
	end
	local each = math.floor((rest_w - MAGIC_GAP * (n - 1)) / n)
	for i, w in ipairs(rest) do
		float_window_to(w, rest_x0 + (i - 1) * (each + MAGIC_GAP), MAGIC_TOP, each, h)
	end
end

local function apply_magic_layout()
	align_fixed_pane("easyhub", MAGIC_EASYHUB_W, "right")
end

-------------------
---- AUTOSTART ----
-------------------

hl.on("hyprland.start", function()
	-- Tray watcher first, so apps export their icons to easyhub's Apps tab.
	hl.exec_cmd(os.getenv("HOME") .. "/.local/bin/easyhub --watcher")
	hl.exec_cmd(bar)
	hl.exec_cmd(wallpaper)
	hl.exec_cmd("systemctl --user start hyprpolkitagent")
	hl.exec_cmd(os.getenv("HOME") .. "/.config/hypr/scripts/monitor-watch.sh")

	-- "magic" special-workspace dashboard, spawned once at startup:
	-- kitty+btop on the left, kitty+easyhub on the right. btop is launched
	-- first so it tiles on the left; easyhub follows after a delay so it tiles
	-- on the right and the tray watcher above has come up first.
	hl.exec_cmd("[workspace special:magic silent] " .. terminal .. " -e btop")
	hl.timer(function()
		hl.exec_cmd("[workspace special:magic silent] " .. terminal .. " -e easyhub")
	end, { type = "oneshot", timeout = 1000 })

	-- Once both dashboard windows exist (and their titles are set), pin them
	-- to the fixed floating layout.
	hl.timer(apply_magic_layout, { type = "oneshot", timeout = 2000 })
end)

-------------------------------
---- ENVIRONMENT VARIABLES ----
-------------------------------

hl.env("XCURSOR_SIZE", "20")
hl.env("HYPRCURSOR_SIZE", "20")
hl.env("GTK_THEME", "Adwaita:dark")
hl.env("QT_QPA_PLATFORMTHEME", "gtk3")
hl.env("QT_STYLE_OVERRIDE", "Adwaita-Dark")

-----------------------
---- LOOK AND FEEL ----
-----------------------

hl.config({
	general = {
		gaps_in = 5,
		gaps_out = 10,
		border_size = 2,
		col = {
			active_border = "rgba(8087d6ee)",
			inactive_border = "rgba(313244ee)",
		},
		resize_on_border = false,
		allow_tearing = false,
		layout = "dwindle",
	},

	decoration = {
		active_opacity = 1.0,
		inactive_opacity = 0.9,
		shadow = {
			enabled = true,
			range = 4,
			render_power = 3,
			color = 0xee1a1a1a,
		},
		blur = {
			enabled = true,
			size = 3,
			passes = 1,
			vibrancy = 0.1696,
		},
	},

	gestures = {
    		workspace_swipe_invert = false
	},

	group = {
		col = {
			border_active = "rgba(8087d6ee)",
			border_inactive = "rgba(313244ee)",
		},
		groupbar = {
			font_family = "CommitMono Nerd Font Bold",
			col = {
				active = "rgba(8087d6ee)",
				inactive = "rgba(313244ee)",
			},
		},
	},

	animations = { enabled = true },

	dwindle = { preserve_split = true },
	master = { new_status = "master" },

	misc = {
		force_default_wallpaper = -1,
		disable_hyprland_logo = false,
	},
})

hl.curve("easeOutQuint", { type = "bezier", points = { { 0.23, 1 }, { 0.32, 1 } } })
hl.curve("easeInOutCubic", { type = "bezier", points = { { 0.65, 0.05 }, { 0.36, 1 } } })
hl.curve("linear", { type = "bezier", points = { { 0, 0 }, { 1, 1 } } })
hl.curve("almostLinear", { type = "bezier", points = { { 0.5, 0.5 }, { 0.75, 1 } } })
hl.curve("quick", { type = "bezier", points = { { 0.15, 0 }, { 0.1, 1 } } })

hl.animation({ leaf = "global", enabled = true, speed = 10, bezier = "default" })
hl.animation({ leaf = "border", enabled = true, speed = 5.39, bezier = "easeOutQuint" })
hl.animation({ leaf = "windows", enabled = true, speed = 4.79, bezier = "easeOutQuint" })
hl.animation({ leaf = "windowsIn", enabled = true, speed = 4.1, bezier = "easeOutQuint", style = "popin 87%" })
hl.animation({ leaf = "windowsOut", enabled = true, speed = 1.49, bezier = "linear", style = "popin 87%" })
hl.animation({ leaf = "fadeIn", enabled = true, speed = 1.73, bezier = "almostLinear" })
hl.animation({ leaf = "fadeOut", enabled = true, speed = 1.46, bezier = "almostLinear" })
hl.animation({ leaf = "fade", enabled = true, speed = 3.03, bezier = "quick" })
hl.animation({ leaf = "layers", enabled = true, speed = 3.81, bezier = "easeOutQuint" })
hl.animation({ leaf = "layersIn", enabled = true, speed = 4, bezier = "easeOutQuint", style = "fade" })
hl.animation({ leaf = "layersOut", enabled = true, speed = 1.5, bezier = "linear", style = "fade" })
hl.animation({ leaf = "fadeLayersIn", enabled = true, speed = 1.79, bezier = "almostLinear" })
hl.animation({ leaf = "fadeLayersOut", enabled = true, speed = 1.39, bezier = "almostLinear" })
hl.animation({ leaf = "workspaces", enabled = true, speed = 6, bezier = "easeOutQuint", style = "slide" })
hl.animation({ leaf = "workspacesIn", enabled = true, speed = 6, bezier = "easeOutQuint", style = "slide" })
hl.animation({ leaf = "workspacesOut", enabled = true, speed = 6, bezier = "easeOutQuint", style = "slide" })
hl.animation({ leaf = "zoomFactor", enabled = true, speed = 7, bezier = "quick" })

---------------
---- INPUT ----
---------------

hl.config({
	input = {
		kb_layout = "us,ru,ua,pl",
		kb_variant = "",
		kb_model = "",
		kb_options = "caps:swapescape",
		kb_rules = "",
		follow_mouse = 1,
		sensitivity = -0.4,
		touchpad = { natural_scroll = false },
	},
})

-- Per-device pointer speeds live in user_config.lua (gitignored, machine-local)
-- since device names are hardware-specific. See user_config.lua.sample.
local ok, user = pcall(dofile, os.getenv("HOME") .. "/.config/hypr/user_config.lua")
if ok and type(user) == "table" and user.devices then
	for _, dev in ipairs(user.devices) do
		hl.device(dev)
	end
end

---------------------
---- KEYBINDINGS ----
---------------------

local mainMod = "SUPER"

hl.bind(mainMod .. " + T", hl.dsp.exec_cmd(terminal))
hl.bind(mainMod .. " + SPACE", hl.dsp.exec_cmd(menu))
hl.bind(mainMod .. " + Q", hl.dsp.window.close())
hl.bind(mainMod .. " + L", hl.dsp.exec_cmd("hyprlock"))

-- Region screenshot to clipboard
hl.bind("Print", hl.dsp.exec_cmd("grimblast --notify copysave area"))

-- Switch keyboard layout (us, ru, ua, pl)
hl.bind(mainMod .. " + F", hl.dsp.exec_cmd("hyprctl switchxkblayout all 0"))
hl.bind(mainMod .. " + D", hl.dsp.exec_cmd("hyprctl switchxkblayout all 1"))
hl.bind(mainMod .. " + S", hl.dsp.exec_cmd("hyprctl switchxkblayout all 2"))
hl.bind(mainMod .. " + A", hl.dsp.exec_cmd("hyprctl switchxkblayout all 3"))

-- Move focus
for _, i in ipairs({ "left", "right", "up", "down" }) do
	hl.bind(mainMod .. " + " .. i, hl.dsp.focus({ direction = i }))
end

-- Move window
for _, i in ipairs({ "left", "right", "up", "down" }) do
	hl.bind(mainMod .. " + CTRL + " .. i, hl.dsp.window.move({ direction = i }))
end

-- Resize window
local resizeAtts = {
	{ key = "left", x = -20, y = 0 },
	{ key = "right", x = 20, y = 0 },
	{ key = "up", x = 0, y = 20 },
	{ key = "down", x = 0, y = -20 },
}
for _, i in ipairs(resizeAtts) do
	hl.bind(mainMod .. " + ALT + " .. i.key, hl.dsp.window.resize({ x = i.x, y = i.y, relative = true }))
end

-- Switch workspaces
for i = 1, 10 do
	local key = i % 10
	hl.bind(mainMod .. " + " .. key, hl.dsp.focus({ workspace = i }))
end

-- Move active window to workspace
for i = 1, 10 do
	local key = i % 10
	hl.bind(mainMod .. " + CTRL + " .. key, hl.dsp.window.move({ workspace = i }))
end

-- Switch groups
for i = 1, 10 do
	local key = i % 10
	hl.bind(mainMod .. " + SHIFT + " .. key, hl.dsp.group.active({index = i}))
end

-- Special workspace "magic" — toggle the kitty+btop / kitty+easyhub dashboard
-- that is spawned once at startup (see AUTOSTART).
hl.bind(mainMod .. " + M", hl.dsp.workspace.toggle_special("magic"))
hl.bind(mainMod .. " + CTRL + M", hl.dsp.window.move({ workspace = "special:magic" }))

-- Float workspace
hl.bind("SUPER + mouse:272", hl.dsp.window.drag(), { mouse = true })
hl.bind("SUPER + mouse:273", hl.dsp.window.resize(), { mouse = true })

-- Touchpad
hl.gesture({ fingers = 3, direction = "horizontal", action = "workspace" })
hl.gesture({ fingers = 3, direction = "swipe", mods = "SUPER", action = "move" })
hl.gesture({ fingers = 4, direction = "swipe", mods = "SUPER", action = "resize" })

-- Laptop multimedia keys
hl.bind(
	"XF86AudioRaiseVolume",
	hl.dsp.exec_cmd("wpctl set-volume -l 1 @DEFAULT_AUDIO_SINK@ 5%+"),
	{ locked = true, repeating = true }
)
hl.bind(
	"XF86AudioLowerVolume",
	hl.dsp.exec_cmd("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-"),
	{ locked = true, repeating = true }
)
hl.bind(
	"XF86AudioMute",
	hl.dsp.exec_cmd("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle"),
	{ locked = true, repeating = true }
)
hl.bind(
	"XF86AudioMicMute",
	hl.dsp.exec_cmd("wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle"),
	{ locked = true, repeating = true }
)
hl.bind("XF86MonBrightnessUp", hl.dsp.exec_cmd("brightnessctl -e4 -n2 set 5%+"), { locked = true, repeating = true })
hl.bind("XF86MonBrightnessDown", hl.dsp.exec_cmd("brightnessctl -e4 -n2 set 5%-"), { locked = true, repeating = true })

hl.bind("XF86AudioNext", hl.dsp.exec_cmd("playerctl next"), { locked = true })
hl.bind("XF86AudioPause", hl.dsp.exec_cmd("playerctl play-pause"), { locked = true })
hl.bind("XF86AudioPlay", hl.dsp.exec_cmd("playerctl play-pause"), { locked = true })
hl.bind("XF86AudioPrev", hl.dsp.exec_cmd("playerctl previous"), { locked = true })

hl.bind(mainMod .. " + G", hl.dsp.group.toggle())

--------------------------------
---- WINDOWS AND WORKSPACES ----
--------------------------------

hl.window_rule({
	name = "suppress-maximize-events",
	match = { class = ".*" },
	suppress_event = "maximize",
})

hl.window_rule({
	name = "fix-xwayland-drags",
	match = {
		class = "^$",
		title = "^$",
		xwayland = true,
		float = true,
		fullscreen = false,
		pin = false,
	},
	no_focus = true,
})

hl.window_rule({
	name = "move-hyprland-run",
	match = { class = "hyprland-run" },
	move = "20 monitor_h-120",
	float = true,
})

for i = 1, 6 do
	hl.workspace_rule({
		workspace = "" .. i,
		persistent = true,
	})
end

local FLOAT_WS = { ["3"] = true, ["5"] = true }

--- @param win HL.Window
local function try_float(win)
	if not win then
		return
	end
	-- Leave the magic dashboard alone; it manages its own floating layout.
	if win.workspace and win.workspace.special then
		return
	end
	if FLOAT_WS[win.workspace.name] then
		hl.dispatch(hl.dsp.window.float({ action = "enable", window = win }))
	else
		hl.dispatch(hl.dsp.window.float({ action = "disable", window = win }))
	end
end
hl.on("window.open", try_float)
hl.on("window.move_to_workspace", try_float)
