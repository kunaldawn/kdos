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
# --root and an A/B update install into a tree that is not the running one, and
# an account made in / instead would be missing from the system that needs it.
#
# The guard reads the files: no getent on musl, and a missing one returns 127,
# so the test never holds and useradd runs against the account it already made.
root="${PKG_ROOT:-/}"
grep -q '^polkitd:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r polkitd
grep -q '^polkitd:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g polkitd -d /nonexistent -s /sbin/nologin polkitd

# polkitd reads the rules as this account, and the directory is 0750 so that a
# hijacked polkitd cannot write them: root owns it, the polkitd group reads it.
# Built without the account, it arrives root:root and every rule is unread.
while IFS=: read -r n _ gid _; do
	[ "$n" = polkitd ] || continue
	install -d -m 750 "$root/etc/polkit-1/rules.d"
	chown "0:$gid" "$root/etc/polkit-1/rules.d"
done < "$root/etc/group"
