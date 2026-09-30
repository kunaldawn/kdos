# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The server runs as this account, and PostgreSQL's peer authentication maps
# it to the database role of the same name, so the account owns the
# attachment store the server writes.
#
# Everything is written under PKG_ROOT, the root kpkgadd is installing into:
# --root and an A/B update install into a tree that is not the running one, and
# an account made in / instead would be missing from the system that needs it.
#
# The guard reads the files: no getent on musl, and a missing one returns 127,
# so the test never holds and useradd runs against the account it already made.
root="${PKG_ROOT:-/}"
grep -q '^gnuhealth:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r gnuhealth
grep -q '^gnuhealth:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g gnuhealth -d /var/lib/gnuhealth -s /sbin/nologin gnuhealth

# chown resolves a name against the running root's passwd, so the ids are read
# out of PKG_ROOT's.
# The mode is set as well: kpkgadd creates every directory 0755, whatever mode
# it was packaged with, because the package carries no group to give it.
while IFS=: read -r n _ uid gid _; do
	[ "$n" = gnuhealth ] || continue
	chown "$uid:$gid" "$root/var/lib/gnuhealth" "$root/var/lib/gnuhealth/attach"
	chmod 750 "$root/var/lib/gnuhealth" "$root/var/lib/gnuhealth/attach"
done < "$root/etc/passwd"
