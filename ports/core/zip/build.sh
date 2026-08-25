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
make -f unix/Makefile generic CC="${CC:-cc} -Wno-implicit-function-declaration"

make -f unix/Makefile prefix=$PKG/usr MANDIR=$PKG/usr/share/man/man1 install
