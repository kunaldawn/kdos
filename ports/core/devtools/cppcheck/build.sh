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

cmake -S . -B build -G Ninja \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D FILESDIR=/usr/share/cppcheck \
	-D BUILD_CLI=ON \
	-D BUILD_GUI=OFF \
	-D BUILD_TESTING=OFF \
	-D HAVE_RULES=OFF \
	-D USE_BUNDLED_TINYXML2=OFF \
	-D USE_MATCHCOMPILER=On \
	-D USE_BOOST=On \
	-D NO_UNIX_BACKTRACE_SUPPORT=ON
cmake --build build
DESTDIR=$PKG cmake --install build

xsltproc --nonet --param man.charmap.use.subset 0 \
	/usr/share/xml/docbook/xsl-stylesheets-[0-9]*/manpages/docbook.xsl man/cppcheck.1.xml
install -Dm644 cppcheck.1 "$PKG/usr/share/man/man1/cppcheck.1"
install -Dm755 htmlreport/cppcheck-htmlreport "$PKG/usr/bin/cppcheck-htmlreport"

test -x "$PKG/usr/bin/cppcheck"
test -f "$PKG/usr/share/cppcheck/cfg/std.cfg"
