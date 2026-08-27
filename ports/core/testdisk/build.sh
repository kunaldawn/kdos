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

# `basename` LIVES IN <libgen.h> ON musl AND NOWHERE ELSE. glibc additionally
# declares a GNU variant in <string.h>, which is what hdaccess.c relies on
# without including anything; here that is an implicit declaration, so the
# compiler assumes it returns `int` and the returned POINTER is truncated to 32
# bits. GCC 15 makes that an error, which is the good outcome — under an older
# compiler it built and corrupted a path. `-include libgen.h` supplies the real
# declaration to every translation unit and is a flag rather than a patch.
export CPPFLAGS="${CPPFLAGS:-} -include libgen.h"

# --without-qt: the qphotorec GUI is the one part of this that would put Qt on
# the host, and the recovery tools are all ncurses. The three library flags are
# what make the difference between reading a damaged ext4 and guessing at it.
./configure \
	--prefix=/usr \
	--without-qt \
	--enable-sudo=no
make
make DESTDIR=$PKG install
