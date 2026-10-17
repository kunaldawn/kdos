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
grep -q '^libvirt:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r libvirt
grep -q '^qemu:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g kvm -d /var/lib/libvirt -s /sbin/nologin qemu
