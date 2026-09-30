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

# unix/configure compiles every feature probe with $CC ALONE and never passes
# $CFLAGS, so a suppression put in CFLAGS cannot reach them. Under this
# compiler an implicit declaration is an error, so each probe answers
# "missing": the generated flags carry -DNO_RMDIR -DNO_STRCHR -DNO_RENAME
# -DNO_MKTEMP -DZMEM -DNO_DIR, zip.h then redeclares memset/memcpy/memcmp with
# K&R prototypes, and the build stops on conflicting types. The flag rides on
# CC because that is the only variable the probes see.
#
# BZIP2 HAS NO FORCING SWITCH: IZ_BZIP2 wants libbz2.a and bzlib.h in one
# directory, and otherwise the OS probe decides alone. `flags` is generated
# first and checked, so a probe that misses the installed libbz2 stops the
# build instead of shipping a zip that cannot write `-Z bzip2` archives.
ZIP_CC="${CC:-cc} -Wno-implicit-function-declaration"
make -f unix/Makefile flags CC="$ZIP_CC"
grep -q -- -DBZIP2_SUPPORT flags || {
	echo "zip: bzip2 probe failed — flags lack -DBZIP2_SUPPORT" >&2
	exit 1
}
make -f unix/Makefile generic CC="$ZIP_CC"

make -f unix/Makefile prefix=$PKG/usr MANDIR=$PKG/usr/share/man/man1 install
