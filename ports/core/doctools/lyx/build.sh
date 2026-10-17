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

./configure --prefix=/usr --sysconfdir=/etc \
	--enable-qt6 \
	--with-hunspell \
	--without-aspell \
	--without-enchant \
	--without-saxon \
	--without-included-mythes \
	--without-included-hunspell \
	--disable-callstack-printing \
	--disable-nls
make
make DESTDIR=$PKG install

for d in "$PKG"/usr/share/lyx/doc/*/ "$PKG"/usr/share/lyx/examples/*/ \
	"$PKG"/usr/share/lyx/templates/*/; do
	case $(basename "$d") in
	[a-z][a-z] | [a-z][a-z]_[A-Z][A-Z]) rm -rf "$d" ;;
	esac
done
