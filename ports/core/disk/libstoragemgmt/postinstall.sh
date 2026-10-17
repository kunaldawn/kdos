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

root="${PKG_ROOT:-/}"
grep -q '^libstoragemgmt:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r libstoragemgmt
grep -q '^libstoragemgmt:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g libstoragemgmt -d /run/lsm -s /sbin/nologin libstoragemgmt
