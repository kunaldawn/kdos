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

# Compress::Raw::Zlib and Compress::Raw::Bzip2 compile their own bundled
# copies of zlib and bzip2 unless told otherwise; these link the system ones,
# which the security database tracks. Their Makefile.PLs read the environment.
export BUILD_ZLIB=False BUILD_BZIP2=0

# -Duseshrplib builds libperl.so. The default is a static, non-PIC libperl.a,
# which a program embedding the interpreter (perf's `perf script` Perl engine)
# cannot link into a PIE.
#
# GDBM_File, NDBM_File and ODBM_File are built when Configure finds gdbm and
# its dbm/ndbm compatibility headers, with no switch to demand them: gdbm in
# depends is what keeps them.
./Configure -des \
	-Dprefix=/usr \
	-Dvendorprefix=/usr \
	-Dprivlib=/usr/share/perl5/core_perl \
	-Darchlib=/usr/lib/perl5/core_perl \
	-Dsitelib=/usr/share/perl5/site_perl \
	-Dsitearch=/usr/lib/perl5/site_perl \
	-Dvendorlib=/usr/share/perl5/vendor_perl \
	-Dvendorarch=/usr/lib/perl5/vendor_perl \
	-Dman1dir=/usr/share/man/man1 \
	-Dman3dir=/usr/share/man/man3 \
	-Duseshrplib \
	-Ud_eaccess -Ud_euidaccess
make
make DESTDIR=$PKG install
