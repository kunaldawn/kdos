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

# The account D-Bus activation starts geoclue as: its .service file says
# User=geoclue, and dbus-daemon-launch-helper refuses to start a service whose
# user does not exist. Created under PKG_ROOT, the root kpkgadd is installing
# into, and guarded by reading the files, as avahi's hook is.
root="${PKG_ROOT:-/}"
grep -q '^geoclue:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r geoclue
grep -q '^geoclue:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g geoclue -d /nonexistent -s /sbin/nologin geoclue

# The guard leaves an existing account as it is, so the home is set apart
# from it: a tree that already has the account takes /nonexistent too. usermod
# exits 0 without writing when the home is already /nonexistent.
usermod -R "$root" -d /nonexistent geoclue
