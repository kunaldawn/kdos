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

cat > "$PKG/etc/chrony/chrony.conf" <<'EOF'
pool pool.ntp.org iburst
driftfile /var/lib/chrony/drift
makestep 1.0 3
rtcsync
EOF
