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

# THE SOURCE RELEASE, NOT THE TAG ARCHIVE — rizin vendors several
# subprojects and the published `rizin-src-*.tar.xz` is the one that carries
# them. Third time this pattern appears in this tree (bcc, yosys, and here),
# and the failure mode is identical each time: an empty directory, a
# configure that succeeds, and a build that dies in a missing header.
#
# ITS DECOMPILER IS WEAK AND THAT IS SAID OUT LOUD. Ghidra is the decompiler
# and it is a boxed row precisely because it is 1.2 GB of Java; what rizin
# gives you natively is disassembly, cross-references, strings and patching in
# a terminal, which is the fast 90% and needs no JVM.
#
# -Duse_sys_capstone: capstone is already a port. Building the bundled one
# would put a second disassembler engine on the machine with its own bugs. The
# same holds for every other use_sys_* that has a port behind it — libmagic,
# lz4, zstd, xz, pcre2 and the tree-sitter runtime — and each is `enabled`, so a
# missing library fails setup instead of falling back to the vendored copy.
# zydis, softfloat, libmspack, blake2 and blake3 are not ports, and their
# defaults build the vendored copies. regenerate_cmds would rebuild cmd_descs
# whenever PyYAML happens to be installed; the release carries them generated.
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
	-Duse_sys_tree_sitter=enabled \
	-Dregenerate_cmds=disabled \
	-Denable_tests=false \
	-Denable_rz_test=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
