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

# Xfce's settings store: libxfconf and xfconfd, the D-Bus-activated daemon
# behind it, which libxfce4ui reads its keyboard shortcuts and dialog state
# from. The GSettings backend module stays unbuilt: installed under
# gio/modules it would offer every GSettings program an xfconf store beside
# dconf. Introspection and Vala are off, as in libxfce4util.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--disable-gsettings-backend \
	--enable-introspection=no \
	--enable-vala=no \
	--disable-gtk-doc \
	--disable-checks \
	--disable-profiling
make
make DESTDIR=$PKG install

# Bundled data is English only; the library falls back to its source strings.
rm -rf "$PKG/usr/share/locale"
