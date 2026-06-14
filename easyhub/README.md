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
- **Power** — shutdown, restart, log out, sleep (with a confirm dialog).

## Runtime tools

Already present on this box: `nmcli`, `bluetoothctl`, `brightnessctl`, `wpctl`.

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
