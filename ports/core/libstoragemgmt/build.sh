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

# configure.ac appends -Werror to CPPFLAGS. CFLAGS and CXXFLAGS follow
# CPPFLAGS on every compile line, so -Wno-error there wins; without it a
# warning a newer compiler adds stops the build.
export CFLAGS="$CFLAGS -Wno-error"
export CXXFLAGS="$CXXFLAGS -Wno-error"

# The SMI-S plugin needs pywbem and ledmon backs LED control; neither is a
# port. The test suite needs check, chrpath and valgrind. The systemd unit,
# tmpfiles and sysusers files are not installed: lsmd is started by nothing
# here. It needs /var/run/lsm to exist, and without a `libstoragemgmt`
# account it keeps the privileges of whoever started it.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--localstatedir=/var \
	--disable-static \
	--without-test \
	--without-smispy \
	--without-ledmon \
	--with-bash-completion-dir=/usr/share/bash-completion/completions \
	--with-systemdsystemunitdir=no \
	--with-systemd-sysusersdir=no \
	--with-systemd-tmpfilesdir=no \
	PYTHON=python3
make
make DESTDIR=$PKG install
