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

ln -s "$SRC_ROOT/sigdb-$_sigdb" subprojects/sigdb

meson setup build \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=lib \
	--buildtype=release \
	-Duse_sys_capstone=enabled \
	-Duse_sys_openssl=enabled \
	-Duse_sys_zlib=enabled \
	-Duse_sys_libzip=enabled \
	-Duse_sys_xxhash=enabled \
	-Duse_sys_magic=enabled \
	-Duse_sys_lz4=enabled \
	-Duse_sys_libzstd=enabled \
	-Duse_lzma=true \
	-Duse_sys_lzma=enabled \
	-Duse_sys_pcre2=enabled \
	-Duse_sys_libmspack=enabled \
	-Duse_sys_tree_sitter=disabled \
	-Dregenerate_cmds=disabled \
	-Dinstall_sigdb=true \
	-Denable_tests=false \
	-Denable_rz_test=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
