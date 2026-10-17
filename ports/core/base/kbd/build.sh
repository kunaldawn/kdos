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

./configure --prefix=/usr --sysconfdir=/etc --disable-nls --disable-tests \
	--enable-vlock \
	--enable-xkb \
	--with-zlib --with-bzip2 --with-lzma --with-zstd
for _h in HAVE_ZLIB HAVE_BZIP2 HAVE_LZMA HAVE_ZSTD; do
	grep -q "^#define $_h 1" config.h || { echo "kbd: $_h missing" >&2; exit 1; }
done
grep -q '^XKBCOMMON_LIBS = .*-lxkbcommon' src/Makefile \
	|| { echo "kbd: xkbcommon missing" >&2; exit 1; }
make
make DESTDIR=$PKG install

rm -rf "$PKG/usr/share/consolefonts"

install -d -m 755 "$PKG/etc/pam.d"
cat > "$PKG/etc/pam.d/vlock" << "EOF"
auth     include system-auth
account  include system-account
EOF
chmod 644 "$PKG/etc/pam.d/vlock"
