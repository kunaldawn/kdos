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

# avahi-daemon drops to this user, and it is created here rather than in
# fs/etc/passwd because 00_file_system.sh MERGES those files: a runtime-added
# service user survives a re-sync, a repo-added one would need the repo edited
# for every package that wants a uid.
getent group avahi >/dev/null 2>&1 || groupadd -r avahi
getent passwd avahi >/dev/null 2>&1 || \
	useradd -r -g avahi -d /run/avahi-daemon -s /sbin/nologin avahi
