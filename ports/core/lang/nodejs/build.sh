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

# V8 reads Temporal's time-zone rules through a private ICU header that a
# system ICU does not install; the patch compiles the rules into the binary
# instead, from the zoneinfo64 crate's resource file. That file is replaced
# with the one the icu port compiles from its own source, so the rules are the
# system ICU's tz release and not a copy shipped in the tarball.
patch -p1 -i $PORT_SRC/v8-temporal-system-icu.patch
install -m644 "/usr/share/icu/$(pkg-config --modversion icu-uc)/zoneinfo64.res" \
	deps/crates/vendor/zoneinfo64-v0_3/src/data/zoneinfo64.res

# --shared-openssl with --openssl-use-def-ca-store: TLS comes from the system
# library and trusts /etc/ssl, not a copy of OpenSSL and a CA list compiled
# into the binary.
#
# --v8-enable-temporal-support turns configure's silent cargo probe into a
# requirement. The crates are vendored under deps/crates and built --frozen.
#
# --shared-sqlite holds node:sqlite to the system library, which it can link
# only while that library is built with the session extension and column
# metadata: node:sqlite calls sqlite3session_* and sqlite3_column_table_name.
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
