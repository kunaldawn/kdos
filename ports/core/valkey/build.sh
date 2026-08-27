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

# REDIS RELICENSED SOURCE-AVAILABLE IN 2024 AND VALKEY IS THE BSD-3
# CONTINUATION. That is not a preference: a distro that compiles everything it
# ships from source and states its licences cannot carry RSAL, and valkey is
# the same codebase under the licence redis had when the ecosystem was built on
# it.
#
# MALLOC=libc IS LOAD-BEARING ON MUSL. The bundled jemalloc assumes glibc's
# malloc internals and its own thread-cache teardown; built against musl it
# compiles and then aborts under load, which is the worst kind of wrong for a
# data store. musl's allocator is slower and correct.
make MALLOC=libc BUILD_TLS=yes PREFIX=/usr
make MALLOC=libc BUILD_TLS=yes PREFIX=/usr DESTDIR=$PKG install

install -Dm644 valkey.conf   $PKG/etc/valkey/valkey.conf
install -Dm644 sentinel.conf $PKG/etc/valkey/sentinel.conf
