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

# The hash methods offered are the ones musl's crypt() implements: SHA-256,
# SHA-512 and bcrypt ($2b$). musl has no yescrypt, and it reads a `$y$` salt
# as a traditional DES one and returns a DES hash, so --with-yescrypt would
# offer `ENCRYPT_METHOD YESCRYPT` and store a weak hash for every password set
# with it. ENCRYPT_METHOD is unset, and unset is SHA512.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--disable-logind \
	--disable-static \
	--enable-lastlog \
	--with-bcrypt \
	--without-yescrypt \
	--without-libbsd \
	--without-libpam \
	--with-acl \
	--with-btrfs \
	--without-audit \
	--without-selinux \
	--without-tcb \
	--with-group-name-max-length=32 

make
make DESTDIR=$PKG install

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
