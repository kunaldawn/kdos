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

# Alpine's patch set: C++ standard headers where the sources name the pre-ISO
# ones, the iomanip.h check configure would otherwise stop on, a bool typedef
# that C23 rejects, the UTF-16 writer, and three memory-safety fixes
# (CVE-2007-4460 among them).
for p in 10-fix-compilation-with-cpp-headers 30-fix-utf16 \
	50-remove-outdated-check 60-id3lib-missing-nullpointer-check \
	61-fix_vbr_stack_smash CVE-2007-4460 bool-typedef; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done

# The shipped configure and libtool are 2003's, and the iomanip.h patch is to
# configure.in, so the build system is regenerated. autoheader is left out:
# the shipped config.h.in is the one the sources expect, and autoheader
# rewrites it from the obsolete acconfig.h.
libtoolize --force --copy --install
aclocal
autoconf
automake --add-missing --copy

# zlib comes from the system: with libz found, the bundled copy under zlib/
# is configured but not built, and nothing from it is installed.
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install
