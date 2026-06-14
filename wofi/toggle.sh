#!/bin/sh
# Wofi launcher:
#  - toggles: a second Super+Space closes the open menu instead of stacking
#  - lists application names in lowercase (GTK3 can't text-transform in CSS)
#  - case-insensitive search
#  - launches terminal apps inside kitty, GUI apps directly
#
# The whole desktop-file parse is a single gawk pass, so startup is instant.

# Toggle off if already open.
if pkill -x wofi; then
	exit 0
fi

TERMINAL="kitty"

# XDG application dirs in precedence order (user dir overrides system dirs).
dirs="$HOME/.local/share/applications /usr/local/share/applications /usr/share/applications /var/lib/flatpak/exports/share/applications $HOME/.local/share/flatpak/exports/share/applications"

# Build the .desktop file argument list with shell globbing only (no forks).
set --
for d in $dirs; do
	for f in "$d"/*.desktop; do
		[ -e "$f" ] && set -- "$@" "$f"
	done
done
[ "$#" -gt 0 ] || exit 0

# One gawk pass: emit "lowercase-name <TAB> path" for visible applications,
# honouring XDG override precedence (first basename wins) and NoDisplay/Hidden.
list=$(awk '
	FNR == 1 {
		bn = FILENAME; sub(/.*\//, "", bn)
		dup = (bn in seen); seen[bn] = 1
		name = ""; type = ""; nodisp = 0; hidden = 0; inentry = 0
	}
	dup { next }
	/^\[Desktop Entry\]/ { inentry = 1; next }
	/^\[/               { inentry = 0; next }
	inentry && /^Name=/      && name == "" { name = substr($0, 6) }
	inentry && /^Type=/                    { type = substr($0, 6) }
	inentry && /^NoDisplay=/ { if (tolower(substr($0, 11)) == "true") nodisp = 1 }
	inentry && /^Hidden=/    { if (tolower(substr($0, 8))  == "true") hidden = 1 }
	ENDFILE {
		if (!dup && name != "" && type == "Application" && !nodisp && !hidden)
			print tolower(name) "\t" FILENAME
	}
' "$@")

chosen=$(printf '%s\n' "$list" | sort -f | cut -f1 | wofi --dmenu --prompt Search --insensitive)
[ -n "$chosen" ] || exit 0

file=$(printf '%s\n' "$list" | awk -F'\t' -v c="$chosen" '$1 == c { print $2; exit }')
[ -n "$file" ] || exit 0

# Build the command from Exec, stripping desktop field codes.
cmd=$(awk -F= '/^Exec=/{ sub(/^Exec=/, ""); print; exit }' "$file" \
	| sed -E 's/%[fFuUdDnNickvm]//g; s/[[:space:]]+$//')
[ -n "$cmd" ] || exit 0

term=$(awk -F= '/^Terminal=/{ print tolower($2); exit }' "$file")
if [ "$term" = "true" ]; then
	setsid -f "$TERMINAL" -e sh -c "$cmd" >/dev/null 2>&1
else
	setsid -f sh -c "$cmd" >/dev/null 2>&1
fi
