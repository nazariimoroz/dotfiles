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
    [[ $EUID -eq 0 ]] && { warn "run as a normal user (the script calls sudo where needed)."; exit 1; }
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

# --- run -----------------------------------------------------------------
need_arch
install_pkgs
link_ly
enable_services
log "Done. Reboot or 'sudo systemctl restart ly@tty2' to start the login screen."
