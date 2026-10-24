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

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--runstatedir=/run \
	--disable-static \
	--without-systemd \
	--without-logger

make
make DESTDIR=$PKG install

install -d "$PKG/etc" "$PKG/var/log"

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
# such line.
rotate_size  1M
rotate_count 5

# No network socket at all. With no `listen` line syslogd otherwise opens UDP
# *:514 and accepts any host's messages into these files. 1 would keep
# forwarding to a remote `@host` action without listening; 2 is neither.
secure_mode 2
EOF
