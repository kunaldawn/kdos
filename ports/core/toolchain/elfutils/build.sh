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

CFLAGS="$CFLAGS -Wno-error -include libgen.h -include rpmatch.h -DFNM_EXTMATCH=0" \
LDFLAGS="$LDFLAGS -lrpmatch" \
    ./configure --prefix=/usr --program-prefix="eu-" \
	--disable-debuginfod --disable-libdebuginfod \
	--disable-stackprof \
	--with-zlib --with-bzlib --with-lzma --with-zstd \
	--with-libarchive
make
make DESTDIR=$PKG install
