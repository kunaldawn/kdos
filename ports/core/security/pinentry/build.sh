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

./configure --prefix=/usr \
	--enable-pinentry-curses \
	--enable-pinentry-tty \
	--enable-pinentry-qt \
	--enable-fallback-curses \
	--disable-pinentry-emacs \
	--disable-pinentry-efl \
	--disable-pinentry-gtk2 \
	--disable-pinentry-gnome3 \
	--disable-pinentry-qt5 \
	--disable-pinentry-qt4 \
	--disable-pinentry-tqt \
	--disable-pinentry-fltk \
	--disable-libsecret
for have in PINENTRY_KGUIADDONS PINENTRY_KWINDOWSYSTEM; do
	grep -q "^#define $have 1" config.h || {
		echo "pinentry: $have not configured" >&2
		exit 1
	}
done
make
make DESTDIR=$PKG install
