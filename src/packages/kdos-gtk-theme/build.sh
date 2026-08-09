#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

kdos-theme gtk "$PKG/usr/share/themes/KDOS" phosphor \
	--src "$PORT_SRC/theme"

# `kdos theme <accent>` re-runs kdos-theme against $HOME, so the vendored
# stylesheet ships with the system. That run is also what produces
# ~/.themes/KDOS, which is the only theme path the appbox can see (it
# shares $HOME, not /usr/share) and is why no /etc/skel copy is installed
# here: 06_packaging/00_theme.sh runs `kdos theme phosphor` against
# /etc/skel and generates it.
cp -a "$PORT_SRC/theme" "$PKG/usr/share/kdos/gtk-theme/theme"

install -Dm644 "$PORT_SRC/LICENSE.notice" \
	"$PKG/usr/share/licenses/kdos-gtk-theme/LICENSE.notice"
