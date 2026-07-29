# Wayland session environment.
# Sourced by /etc/profile on every login. Sets up the runtime directory
# (which would normally be created by elogind/systemd-logind) and the env
# vars Wayland-aware toolkits look for.

uid=$(id -u)
if [ -z "$XDG_RUNTIME_DIR" ]; then
	export XDG_RUNTIME_DIR="/run/user/$uid"
fi
if [ ! -d "$XDG_RUNTIME_DIR" ]; then
	mkdir -p "$XDG_RUNTIME_DIR"
	chmod 0700 "$XDG_RUNTIME_DIR"
	chown "$uid:$uid" "$XDG_RUNTIME_DIR" 2>/dev/null || true
fi

export XDG_SESSION_TYPE=wayland
export XDG_CURRENT_DESKTOP=COSMIC

# Where launchers look for .desktop files. XDG_DATA_HOME must NOT be repeated
# inside XDG_DATA_DIRS — launchers scan both, and every app shows up twice.
export XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-$HOME/.cache}"
export XDG_DATA_DIRS="${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"

# kdos-fetch-app also drops a CLI wrapper per alien app here, so `gimp` works
# from any shell, not just from the launcher.
case ":$PATH:" in
	*":$HOME/.local/bin:"*) ;;
	*) export PATH="$HOME/.local/bin:$PATH" ;;
esac
export QT_QPA_PLATFORM=wayland
export GDK_BACKEND=wayland
export MOZ_ENABLE_WAYLAND=1
export _JAVA_AWT_WM_NONREPARENTING=1
export SDL_VIDEODRIVER=wayland
export CLUTTER_BACKEND=wayland
