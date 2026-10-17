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

lto="-flto=auto -ffat-lto-objects -frandom-seed=zstd"
export CFLAGS="${CFLAGS/-O2/-O3} $lto" CXXFLAGS="${CXXFLAGS/-O2/-O3} $lto"
export LDFLAGS="$LDFLAGS -flto=auto"

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
