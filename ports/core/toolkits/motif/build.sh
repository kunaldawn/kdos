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

autoreconf -fi

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--mandir=/usr/share/man \
	--disable-static \
	--disable-demos \
	--disable-tests \
	--disable-debug \
	--disable-message-catalog \
	--disable-glw \
	--enable-utf8 \
	--with-png \
	--with-jpeg \
	--with-xft \
	--with-xrandr \
	--with-xrender \
	--with-xcursor \
	--with-xinerama=no \
	--with-mwmrcdir=/usr/share/X11/mwm \
	YACC='bison -y'
make
make DESTDIR=$PKG install

rm -f "$PKG/usr/bin/mwm" "$PKG/usr/bin/xmbind" \
	"$PKG/usr/share/man/man1/mwm.1" "$PKG/usr/share/man/man1/xmbind.1" \
	"$PKG/usr/share/man/man4/mwmrc.4" \
	"$PKG/usr/share/X11/mwm/system.mwmrc"
rmdir "$PKG/usr/share/X11/mwm"
