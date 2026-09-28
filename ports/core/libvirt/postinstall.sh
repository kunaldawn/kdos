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

# `libvirt` is the group the installed polkit rule lets manage
# qemu:///system without a password; with no such group the rule matches
# nobody. `qemu` is the account system-mode guests run as (-Dqemu_user), in
# the kvm group that opens /dev/kvm; libvirtd refuses to start a guest as an
# account that does not exist. Created under PKG_ROOT, the root kpkgadd is
# installing into, and guarded by reading the files.
root="${PKG_ROOT:-/}"
grep -q '^libvirt:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r libvirt
grep -q '^qemu:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g kvm -d /var/lib/libvirt -s /sbin/nologin qemu
