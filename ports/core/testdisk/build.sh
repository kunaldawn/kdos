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

# --disable-qt: the qphotorec GUI is the one part of this that would put Qt on
# the host, and the recovery tools are all ncurses.
#
# EVERY LIBRARY IS NAMED, because configure's default is "use it if found". An
# explicit --with-X makes a missing library a configure error for ext2fs, jpeg,
# ewf, zlib and iconv; ntfs3g's own check tests a misspelt variable and never
# fires, so config.h is checked for it below. --without-ntfs and
# --without-reiserfs are the legacy libntfs and progsreiserfs, neither of which
# is a port; ntfs-3g covers NTFS.
#
# THE TWO ac_cv_func_libewf_* ARE NOT OVERRIDES OF A REAL ANSWER. configure
# tests for those functions without -lewf on the link line, so both come out
# "no" against a libewf that has them, and ewf.c then fails on implicit
# declarations. Without --with-ewf there are no E01 forensic images.
./configure \
	--prefix=/usr \
	--disable-qt \
	--enable-sudo=no \
	--with-ncurses \
	--with-ext2fs \
	--with-jpeg \
	--with-ntfs3g \
	--without-ntfs \
	--without-reiserfs \
	--with-ewf \
	--with-iconv \
	--with-zlib \
	--with-uuid \
	ac_cv_func_libewf_handle_read_buffer_at_offset=yes \
	ac_cv_func_libewf_handle_write_buffer_at_offset=yes
grep -q '^#define HAVE_LIBNTFS3G 1' config.h || { echo "testdisk: ntfs-3g not configured" >&2; exit 1; }
make
make DESTDIR=$PKG install
