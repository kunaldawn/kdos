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

# Spell checking is hunspell's alone, against the system dictionaries; MyThes
# is the system library. --without-saxon keeps out the prebuilt Saxon .jar
# that the tarball carries for EPUB export, which then runs through xsltproc.
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

# Bundled data is English only: the manuals, examples and templates keep their
# English copies, and every per-language directory (de, pt_BR, zh_CN, ...)
# goes.
for d in "$PKG"/usr/share/lyx/doc/*/ "$PKG"/usr/share/lyx/examples/*/ \
	"$PKG"/usr/share/lyx/templates/*/; do
	case $(basename "$d") in
	[a-z][a-z] | [a-z][a-z]_[A-Z][A-Z]) rm -rf "$d" ;;
	esac
done
