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

# sysklogd 2.x, not the 1.5.1 most distros still carry. 2.x rotates in-process,
# which is the whole reason logrotate is not a KDOS port: one fewer daemon, one
# fewer cron job, and no window where a rotation races the writer.
#
# klogd is a separate binary here and KDOS does not need it — the kernel ring
# buffer is read straight from /proc/kmsg by syslogd itself in 2.x.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--runstatedir=/run \
	--disable-static \
	--without-systemd

make
make DESTDIR=$PKG install

# sysklogd installs its sample config to /usr/share/doc and never creates
# /etc, so this has to exist before the heredoc below writes into it.
install -d "$PKG/etc" "$PKG/var/log"

# Rotation is per-file and in-process: `rotate_size` / `rotate_count` after the
# path. 1M x 5 keeps a boot's worth of history on a live ISO without filling a
# tmpfs — remember /var/log is RAM on the live medium, so an unbounded log is
# not a disk-space bug, it is an OOM.
cat > "$PKG/etc/syslog.conf" <<'EOF'
# facility.level                        destination
auth,authpriv.*                          /var/log/auth.log
*.*;auth,authpriv.none                  -/var/log/messages
kern.*                                  -/var/log/kern.log
cron.*                                  -/var/log/cron
*.=emerg                                *

# Rotation is GLOBAL directives, not per-file suffixes. Writing
# `/var/log/messages rotate_size=1M` puts the path where a facility.priority
# selector belongs and syslogd answers `unknown priority name ""` for every
# such line — measured on a real boot, not guessed.
#
# 1M x 5 keeps a boot's worth of history without filling a tmpfs: on the live
# medium /var/log is RAM, so an unbounded log is an OOM, not a disk-space bug.
rotate_size  1M
rotate_count 5
EOF
