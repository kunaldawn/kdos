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

./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-threads=posix --enable-thread-local-alloc \
	--enable-cplusplus --with-libatomic-ops=none
make
make DESTDIR=$PKG install
install -d "$PKG/usr/share/licenses/$name"
sed -n '/^## Copyright & Warranty/,$p' README.md > "$PKG/usr/share/licenses/$name/LICENSE"
grep -q 'ABSOLUTELY NO WARRANTY' "$PKG/usr/share/licenses/$name/LICENSE"
