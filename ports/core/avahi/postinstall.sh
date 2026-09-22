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
#
# The guard reads the files: no getent on musl, and a missing one returns 127,
# so the test never holds and useradd runs against the account it already made.
grep -q '^avahi:' /etc/group 2>/dev/null || groupadd -r avahi
grep -q '^avahi:' /etc/passwd 2>/dev/null || \
	useradd -r -g avahi -d /run/avahi-daemon -s /sbin/nologin avahi
