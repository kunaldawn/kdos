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

patch -p1 -i "$PORT_SRC/ck-printerr-newline.patch"

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib --localstatedir=/var \
	--buildtype=release \
	-Dauthfw=pam \
	-Dpam_prefix=/etc/pam.d \
	-Dsession_tracking=ConsoleKit \
	-Dintrospection=false \
	-Dgtk_doc=false \
	-Dman=true \
	-Dgettext=false \
	-Dexamples=false \
	-Dtests=false \
	-Dos_type=lfs
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

rm -rf "$PKG/usr/lib/systemd" "$PKG/usr/lib/sysusers.d" "$PKG/usr/lib/tmpfiles.d"
rm "$PKG/usr/share/dbus-1/system-services/org.freedesktop.PolicyKit1.service"
