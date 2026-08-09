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

# musl needs _LARGEFILE64_SOURCE to expose ino64_t/off64_t used in
# src/posix-io.c's dirent64 struct (glibc defines them unconditionally).
export CFLAGS="${CFLAGS:-} -D_LARGEFILE64_SOURCE"
./configure --prefix=/usr --disable-gpg-test --disable-gpgsm-test --disable-g13-test
make
make DESTDIR=$PKG install
