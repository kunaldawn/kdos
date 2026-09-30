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

./autogen.sh
# --with-elfutils is forced so a missing libdwfl fails the build instead of
# quietly dropping the -w backtraces. libunwind is the other unwinder and
# configure refuses both at once; libdwfl is kept because it also gives each
# frame its source file and line. selinux has no port and is pinned off.
./configure --prefix=/usr --mandir=/usr/share/man \
	--with-elfutils \
	--without-libunwind \
	ac_cv_lib_selinux_security_get_boolean_active=no \
	CPPFLAGS="-D__PRI64_PREFIX=__PRI64"
make
make DESTDIR=$PKG install
