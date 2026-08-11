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

install -d "$PKG/var/log"

# Rotation is per-file and in-process: `rotate_size` / `rotate_count` after the
# path. 1M x 5 keeps a boot's worth of history on a live ISO without filling a
# tmpfs — remember /var/log is RAM on the live medium, so an unbounded log is
# not a disk-space bug, it is an OOM.
cat > "$PKG/etc/syslog.conf" <<'EOF'
# facility.level                        destination
*.info;authpriv.none;cron.none          -/var/log/messages
authpriv.*                              /var/log/secure
cron.*                                  -/var/log/cron
kern.*                                  -/var/log/kern.log
*.emerg                                 :omusrmsg:*

# rotation, handled by syslogd itself
/var/log/messages       rotate_size=1M  rotate_count=5
/var/log/secure         rotate_size=1M  rotate_count=5
/var/log/cron           rotate_size=1M  rotate_count=5
/var/log/kern.log       rotate_size=1M  rotate_count=5
EOF
