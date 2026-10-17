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

export LDFLAGS="$LDFLAGS -Wl,--strip-debug"
export CFLAGS="${CFLAGS/-O2/-O3} -DUSE_MALLOC_USABLE_SIZE"
make MALLOC=libc BUILD_TLS=yes USE_SYSTEMD=no DEBUG= PREFIX=/usr
make MALLOC=libc BUILD_TLS=yes USE_SYSTEMD=no DEBUG= PREFIX=$PKG/usr install

install -Dm644 valkey.conf   $PKG/etc/valkey/valkey.conf
install -Dm644 sentinel.conf $PKG/etc/valkey/sentinel.conf
