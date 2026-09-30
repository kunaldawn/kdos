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

# Everything is written under PKG_ROOT, the root kpkgadd is installing into:
# --root and an A/B update install into a tree that is not the running one.
#
# BrlAPI admits a client through polkit's org.a11y.brlapi.write-display, and
# the rule build.sh installs grants that action to the brlapi group and to
# nobody else: without the group every BrlAPI client — brltty-clip included —
# is refused by the running daemon. The desktop account joins it only when the
# group is first made, so a user who later leaves it stays out across
# reinstalls; the guard reads the files because musl has no getent.
root="${PKG_ROOT:-/}"
if ! grep -q '^brlapi:' "$root/etc/group" 2>/dev/null; then
	groupadd -R "$root" -r brlapi
	grep -q '^kdos:' "$root/etc/passwd" 2>/dev/null && \
		usermod -R "$root" -a -G brlapi kdos
fi
