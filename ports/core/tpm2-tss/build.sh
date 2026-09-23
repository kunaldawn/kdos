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


# THE TPM DEVICE NODE IS ROOT'S AND THE STACK RUNS AS THE CALLER. There is no
# tpm2-abrmd here — the kernel's own resource manager at /dev/tpmrm0 is what
# serialises access, which is the arrangement that needs no daemon.
#
# udev RULES SET THE GROUP. Without them /dev/tpmrm0 is 0600 root and every
# call fails as a permission error rather than as a missing chip.
#
# THE tss ACCOUNT THOSE RULES NAME IS fs/etc/passwd's, never this install's.
# Upstream's install-dirs runs useradd/groupadd only when DESTDIR is empty, so
# staging into $PKG leaves the build host's accounts alone. The two dir flags
# keep sysusers.d and tmpfiles.d out of the package even on a build host that
# has systemd's tools.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--localstatedir=/var \
	--disable-static \
	--disable-doxygen-doc \
	--with-udevrulesdir=/usr/lib/udev/rules.d \
	--with-runstatedir=/run \
	--with-sysusersdir=no \
	--with-tmpfilesdir=no
make
make DESTDIR=$PKG install
rm -rf "$PKG/run"
