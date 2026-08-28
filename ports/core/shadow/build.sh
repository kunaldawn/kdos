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
	--disable-logind \
	--disable-nsl \
	--disable-static \
	--enable-lastlog \
	--with-yescrypt \
	--without-libbsd \
	--without-libpam \
	--with-group-name-max-length=32 

make
make DESTDIR=$PKG install

mkdir -p $PKG/bin $PKG/etc/cron/daily
install -m 755 $PORT_SRC/pwck $PKG/etc/cron/daily

# ROOTLESS PODMAN CANNOT MAP A UID WITHOUT THESE TWO BEING PRIVILEGED, and
# every box on this distro is rootless podman. `newuidmap` writes
# /proc/<pid>/uid_map for the user namespace a container runs in, which the
# kernel allows only from a process already holding CAP_SETUID; podman checks
# the binary first and refuses with
#
#   cannot set up namespace using "/usr/bin/newuidmap":
#   should have setuid or have filecaps setuid
#
# then exits 125. Upstream's own `make install` already sets this; the line is
# here so that a configure change cannot drop it silently, because nothing
# downstream would notice until no box on the machine started.
chmod 4755 "$PKG/usr/bin/newuidmap" "$PKG/usr/bin/newgidmap"
