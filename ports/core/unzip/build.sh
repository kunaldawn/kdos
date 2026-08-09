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

make -f unix/Makefile linux_noasm \
	LOCAL_UNZIP="-DBSD -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -include time.h" \
	prefix=$PKG/usr

make prefix=$PKG/usr MANDIR=$PKG/usr/share/man/man1 -f unix/Makefile install
