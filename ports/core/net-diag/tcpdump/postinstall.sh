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
# tcpdump is built --with-user=tcpdump: run as root it switches to this
# account after opening the capture device, and exits if the account is
# missing.
root="${PKG_ROOT:-/}"
grep -q '^tcpdump:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r tcpdump
grep -q '^tcpdump:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g tcpdump -d / -s /sbin/nologin tcpdump
