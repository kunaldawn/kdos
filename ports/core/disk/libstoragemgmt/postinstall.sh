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


# lsmd started as root drops to this account for every plugin that does not
# need root; without it the plugins keep root. Created under PKG_ROOT, the
# root kpkgadd is installing into, and guarded by reading the files.
root="${PKG_ROOT:-/}"
grep -q '^libstoragemgmt:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r libstoragemgmt
grep -q '^libstoragemgmt:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g libstoragemgmt -d /run/lsm -s /sbin/nologin libstoragemgmt
