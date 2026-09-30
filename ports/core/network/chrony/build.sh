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

# chrony's configure is hand-written, not autoconf, and its option names do not
# follow the usual pattern — the runtime and state directories are
# --chronyrundir / --chronyvardir, and readline is --disable-readline while
# editline is --without-editline. Check ./configure --help before adding to
# this list; a flag it does not recognise is ignored silently.
#
# --disable-sechash would drop NTS and authenticated NTP, so gnutls+nettle stay;
# nettle is the hash and AES-GCM-SIV provider, and NSS and libtomcrypt are named
# off so neither can take its place. Every probe here falls back silently, so
# `depends` is what holds a feature on.
#
# editline is libedit's editline/readline.h and gives an interactive chronyc
# history and line editing; there is no GNU readline path in this configure.
# Timestamping needs only linux/net_tstamp.h and lets chronyd use kernel and NIC
# receive/transmit timestamps where the driver offers them.
#
# Off: no libcap (chronyd runs as root under ksvc), no seccomp (Alpine's own
# musl build skips it).
./configure \
	--prefix=/usr \
	--sysconfdir=/etc/chrony \
	--localstatedir=/var \
	--chronyrundir=/run/chrony \
	--chronyvardir=/var/lib/chrony \
	--without-libcap \
	--without-seccomp \
	--disable-scfilter \
	--without-nss \
	--without-tomcrypt

make
make DESTDIR=$PKG install

install -d "$PKG/etc/chrony" "$PKG/var/lib/chrony"

# The kernel already owns the RTC in both directions: CONFIG_RTC_HCTOSYS sets
# the clock from it at boot and CONFIG_RTC_SYSTOHC writes back every 11 minutes.
# `rtcsync` tells chronyd to let the kernel keep doing that instead of taking
# the RTC over itself — without it the two fight and the hardware clock drifts.
#
# `makestep` matters more than it looks: a live ISO with a dead CMOS battery can
# boot years off, and chrony will only slew by default, which means TLS keeps
# failing for hours. Three big corrections, then slew normally.
cat > "$PKG/etc/chrony/chrony.conf" <<'EOF'
pool pool.ntp.org iburst
driftfile /var/lib/chrony/drift
makestep 1.0 3
rtcsync
EOF
