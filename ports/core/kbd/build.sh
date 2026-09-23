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

# EVERY DECOMPRESSOR AND xkb ARE CHECKED AFTER configure, because --with-* and
# --enable-xkb only say "try": a library pkg-config cannot find is dropped with
# no error, and loadkeys then fails on a .gz or .zst keymap, or on an XKB layout
# name, as a file it cannot read.
./configure --prefix=/usr --disable-nls --disable-tests \
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

# THE CONSOLE FONT IS terminus-font's ter-kdos32n, loaded by kdos-getty, and
# the boot menu's is terminus-font's ter-i16n. kbd's own fonts are a second
# 200-odd faces in the same directory that nothing names.
rm -rf "$PKG/usr/share/consolefonts"

# vlock AUTHENTICATES AS SERVICE `vlock`. Without this file PAM falls through
# to `other`, which denies, and the console stays locked against the right
# password. vlock is not setuid: pam_unix checks the password through
# unix_chkpwd.
install -d -m 755 "$PKG/etc/pam.d"
cat > "$PKG/etc/pam.d/vlock" << "EOF"
auth     include system-auth
account  include system-account
EOF
chmod 644 "$PKG/etc/pam.d/vlock"
