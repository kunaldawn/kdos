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

# harfbuzz=dynamic: the autohinter dlopen()s libharfbuzz.so.0 at run time,
# compiled against headers bundled in src/autofit. Linking it instead would be
# a depends cycle — harfbuzz builds against freetype2. librsvg only feeds the
# ft2demos programs, which are not built here.
./configure --prefix=/usr --enable-freetype-config \
	--with-zlib=yes \
	--with-bzip2=yes \
	--with-png=yes \
	--with-brotli=yes \
	--with-harfbuzz=dynamic \
	--with-librsvg=no
make
make DESTDIR=$PKG install
