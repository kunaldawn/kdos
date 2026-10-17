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

patch -p1 -i $PORT_SRC/v8-temporal-system-icu.patch
install -m644 "/usr/share/icu/$(pkg-config --modversion icu-uc)/zoneinfo64.res" \
	deps/crates/vendor/zoneinfo64-v0_3/src/data/zoneinfo64.res

export CFLAGS="${CFLAGS/-O2/-O3}" CXXFLAGS="${CXXFLAGS/-O2/-O3}"
./configure \
	--prefix=/usr \
	--shared-cares \
	--shared-zlib \
	--shared-libuv \
	--shared-openssl \
	--openssl-use-def-ca-store \
	--shared-zstd \
	--shared-ffi \
	--shared-brotli \
	--shared-nghttp2 \
	--shared-sqlite \
	--v8-enable-temporal-support \
	--with-intl=system-icu
make
make DESTDIR=$PKG install
for d in "$PKG"/usr/lib/node_modules/npm/man/man*; do
	install -Dm644 -t "$PKG/usr/share/man/${d##*/}" "$d"/*
done
