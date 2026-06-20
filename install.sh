#!/usr/bin/env bash
#
# install.sh — bootstrap for this ~/.config desktop setup.
# Installs ONLY the packages referenced by the configs in ~/.config
# (ly, waybar, wofi, hypr, dunst, kitty + the tools they invoke + the font),
# then recreates the symlinks those configs rely on.
#
# Safe to re-run: package install uses --needed, symlinks are idempotent.

set -euo pipefail

CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}"

# --- packages, grouped by what uses them ---------------------------------
PKGS=(
    # configured apps
    hyprland          # hypr/hyprland.lua (hyprctl)
    hyprlock          # hypr/hyprlock.conf + win+L bind
    hyprpaper         # hypr/hyprpaper.conf
    ly                # ly/config.ini (login screen)
    waybar            # waybar/
    wofi              # wofi/
    dunst             # dunst/dunstrc
    kitty             # hypr: terminal = kitty

    # tools invoked from the configs
    btop              # hypr (magic dashboard: kitty -e btop)
    brightnessctl     # ly + hypr + waybar (brightness keys)
    playerctl         # hypr (media keys)
    grimblast-git     # hypr (Print -> screenshot)
    wireplumber       # hypr (wpctl volume)
    libnotify         # grimblast --notify (notify-send)
    networkmanager    # waybar (nmcli)
    bluez-utils       # waybar (bluetoothctl)
    libpulse          # waybar (pactl)
    jq                # waybar scripts

    # font used across wofi / waybar / dunst / hyprlock
    otf-commit-mono-nerd
)

# --- helpers -------------------------------------------------------------
log()  { printf '\033[1;35m::\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n' "$*" >&2; }

need_arch() {
    command -v pacman >/dev/null || { warn "pacman not found — Arch-based distro required."; exit 1; }
    if [[ $EUID -eq 0 ]]; then
        warn "run as a normal user (the script calls sudo where needed)."
        exit 1
    fi
}

install_pkgs() {
    local repo=() aur=()
    for p in "${PKGS[@]}"; do
        if pacman -Si "$p" &>/dev/null; then repo+=("$p"); else aur+=("$p"); fi
    done

    if ((${#repo[@]})); then
        log "Installing repo packages: ${repo[*]}"
        sudo pacman -S --needed "${repo[@]}"
    fi

    if ((${#aur[@]})); then
        log "AUR packages needed: ${aur[*]}"
        if   command -v paru >/dev/null; then paru -S --needed "${aur[@]}"
        elif command -v yay  >/dev/null; then yay  -S --needed "${aur[@]}"
        else warn "No AUR helper (paru/yay). Install manually: ${aur[*]}"; fi
    fi
}

# Recreate /etc/ly/config.ini -> ~/.config/ly/config.ini (the only symlink in use)
link_ly() {
    local src="$CONFIG_DIR/ly/config.ini" dst="/etc/ly/config.ini"
    [[ -f $src ]] || { warn "missing $src — skipping ly symlink."; return; }

    if [[ -L $dst && "$(readlink -f "$dst")" == "$src" ]]; then
        log "ly symlink already correct."
        return
    fi
    if [[ -e $dst && ! -L $dst ]]; then
        log "backing up existing $dst -> $dst.bak"
        sudo cp -n "$dst" "$dst.bak"
    fi
    log "linking $dst -> $src"
    sudo mkdir -p /etc/ly
    sudo ln -sfn "$src" "$dst"
}

enable_services() {
    log "Enabling ly login screen (ly@tty2.service)"
    sudo systemctl enable ly@tty2.service
}

# Recommend a fully-English locale, then ask before applying.
#
# A common installer outcome is a "split" locale: LANG=en_US.UTF-8 for the UI
# but per-category LC_*=<region>.UTF-8 (e.g. pl_PL) for formats. That makes
# weekday/month names render in the regional language — e.g. easyhub's clock
# showed "pią 19 cze" instead of "Fri 19 Jun". Date/numbers are only a *format*;
# the actual time is unchanged.
setup_locale() {
    # Nothing to do if every category is already English/C.
    if ! locale | grep -qvE '=("?(en_US\.UTF-8|C|POSIX)"?)?$'; then
        log "Locale already fully English."
        return
    fi

    warn "Locale has non-English category overrides (e.g. LC_TIME) — these make"
    warn "dates/numbers render in the regional language (easyhub clock, waybar,"
    warn "etc.). Recommended fix: pin everything to English (LANG=en_US.UTF-8)."
    read -rp "$(printf '\033[1;35m::\033[0m Set locale to English now? [y/N] ')" reply
    [[ ${reply,,} == y* ]] || { log "Skipping locale change."; return; }

    # Make sure the English locale is actually generated first.
    if ! locale -a 2>/dev/null | grep -qiE '^en_US\.utf-?8$'; then
        log "Generating en_US.UTF-8 locale"
        sudo sed -i 's/^#\s*\(en_US\.UTF-8 UTF-8\)/\1/' /etc/locale.gen
        sudo locale-gen
    fi
    # NOTE: `localectl set-locale LANG=...` only adds/updates LANG and keeps any
    # existing LC_* overrides, so drop those lines first; then pin LANG.
    sudo sed -i '/^LC_/d' /etc/locale.conf 2>/dev/null || true
    sudo localectl set-locale LANG=en_US.UTF-8
    log "Locale set to en_US.UTF-8 — re-login (or restart the session) to apply."
}

# Build the easyhub TUI (FTXUI) and optionally symlink it into ~/.local/bin.
build_easyhub() {
    local src="$CONFIG_DIR/easyhub" bin="$HOME/.local/bin/easyhub"
    [[ -d $src ]] || { warn "missing $src — skipping easyhub build."; return; }

    read -rp "$(printf '\033[1;35m::\033[0m Compile easyhub now? [y/N] ')" reply
    if [[ ${reply,,} == y* ]]; then
        log "Installing easyhub build deps: cmake pkgconf glib2"
        sudo pacman -S --needed cmake pkgconf glib2
        log "Building easyhub (cmake)"
        cmake -S "$src" -B "$src/build" -DCMAKE_BUILD_TYPE=Release
        cmake --build "$src/build" -j
    else
        log "Skipping easyhub build."
    fi

    [[ -x "$src/build/easyhub" ]] || { warn "no easyhub binary at $src/build/easyhub — skipping symlink."; return; }

    read -rp "$(printf '\033[1;35m::\033[0m Symlink easyhub into ~/.local/bin? [y/N] ')" reply
    if [[ ${reply,,} == y* ]]; then
        mkdir -p "$HOME/.local/bin"
        ln -sfn "$src/build/easyhub" "$bin"
        log "Linked $bin -> $src/build/easyhub"
    else
        log "Skipping easyhub symlink."
    fi
}

# --- run -----------------------------------------------------------------
need_arch
install_pkgs
link_ly
enable_services
setup_locale
build_easyhub
log "Done. Reboot or 'sudo systemctl restart ly@tty2' to start the login screen."
