# easyhub

A fast TUI control hub for Hyprland/Wayland. One screen for your PC at a glance,
plus quick Wi-Fi and Bluetooth connecting.

```
⚡ easyhub
        Stats     Wi-Fi     Bluetooth
─────────────────────────────────────────
 Sat 14 Jun  21:03:11
 Battery    ███████████░░░  78%
 Volume     ██████░░░░░░░░  45%
 Brightness █████████░░░░░  66%
 CPU        ███░░░░░░░░░░░  12%
 Memory     ██████░░░░░░░░  41%
 Wi-Fi      MyNet (72%)
 Bluetooth  WH-1000XM4
 ready                  Tab/←→ switch · q quit
```

## Features

- **Stats** — battery, volume (+mute), brightness, CPU, memory, live clock,
  current Wi-Fi and connected Bluetooth devices. Refreshes every 2s.
  Adjust volume/brightness with the on-screen buttons or hotkeys.
- **Wi-Fi** — scan, pick a network, type a password, connect (`nmcli`).
- **Bluetooth** — scan, connect / disconnect (`bluetoothctl`).
- **Apps** — every open window **and** every system-tray app in one list.
  `Enter` on a window focuses it; `Enter` on a tray app opens its tray menu
  (e.g. *Open*) which raises the window — exactly like clicking the tray icon.
  Works for background apps with no visible window (Filen, Telegram, …).
  `r` refreshes, `Esc` backs out of a tray menu.
- **Power** — shutdown, restart, log out, sleep (with a confirm dialog).

## System tray (StatusNotifier)

Hyprland has no tray, so easyhub ships its own `StatusNotifierWatcher` + host:

```sh
easyhub --watcher      # the daemon (autostarted at login from hypr/hyprland.lua)
```

Tray apps export their icon only if a watcher is running **when they start**, so
the watcher is autostarted before everything else. Apps launched before any
watcher existed need one restart to appear. The TUI also spawns the watcher
itself if none is running.

Debug helpers: `easyhub --list` dumps tray items + their menus;
`easyhub --open <substr>` triggers the *Open* entry of a matching tray app.

## Runtime tools

Already present on this box: `nmcli`, `bluetoothctl`, `brightnessctl`, `wpctl`,
`hyprctl`, `jq`. Tray support links `gio-2.0` / `gio-unix-2.0` (GLib).

## Build & run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/easyhub
```

CMake uses the system `ftxui-git` package if installed, otherwise it fetches
FTXUI v5 automatically.

Optional system FTXUI (smaller, faster configure):

```sh
sudo pacman -S ftxui-git
```

A `easyhub` symlink in `~/.local/bin` points at `build/easyhub`, so you can
launch it from anywhere by typing `easyhub`.

## Controls

- `Tab` / `←` `→` — switch tabs
- `↑` `↓` / `Enter` — navigate & select inside a tab
- On **Stats**: `-` / `+` volume, `m` mute, `[` / `]` brightness
- `q` or `Esc` — quit (`Esc` cancels the power dialog first)

## Power actions

| Action   | Command                  |
|----------|--------------------------|
| Shutdown | `systemctl poweroff`     |
| Restart  | `systemctl reboot`       |
| Log out  | `hyprctl dispatch exit`  |
| Sleep    | `systemctl suspend`      |
