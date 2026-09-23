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

# --disable-nls: the image ships no message catalogues, and with NLS on the
# result follows the build root: gettext's libintl.h maps every call to
# libintl_gettext, which only the libintl port defines, and neither is
# declared, so in 03_phase3, where python3 pulls this in, whether gdbm links
# libintl would follow the order the two happen to be built in.
./configure --prefix=/usr \
            --enable-libgdbm-compat \
            --with-readline \
            --disable-nls
make
make DESTDIR=$PKG install
