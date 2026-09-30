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

# The account the udev rule starts usbmuxd as (`--user usbmux`) and gives the
# device node to (OWNER="usbmux"). With no such account the daemon refuses to
# start and no iPhone is ever reachable. Created under PKG_ROOT, the root
# kpkgadd is installing into, and guarded by reading the files.
root="${PKG_ROOT:-/}"
grep -q '^usbmux:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r usbmux
grep -q '^usbmux:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g usbmux -d /var/lib/lockdown -s /sbin/nologin usbmux
