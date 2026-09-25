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

# THE LEGACY DECODERS ARE NAMED HERE, NOT FOUND BY THE MAKEFILE. libzstd.mk
# collects them with `ls lib/legacy/*.c | grep`, which depends on what `ls` and
# `grep` print in the build environment. A command-line ZSTD_LEGACY_FILES
# overrides that assignment in lib/ and programs/ alike, and absolute paths
# resolve from either directory. The list must match ZSTD_LEGACY_SUPPORT: 5
# decodes frames written by zstd v0.5 to v0.7.
#
# THE CLI FORMATS AND THREADS ARE FORCED ON. programs/Makefile probes zlib, xz,
# lz4 and pthread by compiling a test file and quietly drops whatever fails;
# HAVE_*=1 skips the probe, so a missing library is a link error instead of a
# zstd without --format=gzip, xz or lz4, or a single-threaded `zstd -T`.
legacy=("$PWD"/lib/legacy/zstd_v0[5-7].c)
zstd_vars=(
	ZSTD_LEGACY_SUPPORT=5
	ZSTD_LEGACY_FILES="${legacy[*]}"
	HAVE_PTHREAD=1
	HAVE_ZLIB=1
	HAVE_LZMA=1
	HAVE_LZ4=1
)

make "${zstd_vars[@]}"
make "${zstd_vars[@]}" PREFIX=/usr DESTDIR=$PKG install
