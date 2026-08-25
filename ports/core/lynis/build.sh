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

# POSIX SHELL, SO THERE IS NOTHING TO COMPILE — the install is where the
# decisions are. lynis resolves its own include tree relative to the binary
# unless told otherwise, so the wrapper passes the absolute path; running it
# out of /usr/share directly is what upstream expects and what this preserves.
#
# EXPECT NOISY FINDINGS. It scores a systemd/PAM/auditd machine and this one
# has none of those by rule, so a long list of "not found" is the tool being
# right about a system it has never seen — the report's explanations are the
# value here, not the score.
install -dm755 $PKG/usr/share/lynis
cp -a db include plugins default.prf lynis $PKG/usr/share/lynis/
install -Dm644 lynis.8 $PKG/usr/share/man/man8/lynis.8

install -dm755 $PKG/usr/bin
cat > $PKG/usr/bin/lynis <<'SH'
#!/bin/sh
exec /usr/share/lynis/lynis --pentest "$@"
SH
chmod 755 $PKG/usr/bin/lynis
