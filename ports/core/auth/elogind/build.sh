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

for p in "$PORT_SRC"/0*.patch "$PORT_SRC/no-runtime-dir.patch" "$PORT_SRC/lxml-resolve-entities.patch" "$PORT_SRC/udevadm-path.patch"; do
	patch -p1 --no-backup-if-mismatch -i "$p"
done

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --localstatedir=/var \
	--libexecdir=/usr/lib/elogind \
	--buildtype=release \
	-Dmode=release \
	-Dcgroup-controller=elogind \
	-Ddefault-hierarchy=unified \
	-Ddefault-kill-user-processes=false \
	-Dhalt-path=/sbin/halt \
	-Dpoweroff-path=/sbin/poweroff \
	-Dreboot-path=/sbin/reboot \
	-Dkexec-path=/usr/sbin/kexec \
	-Dnologin-path=/usr/sbin/nologin \
	-Dpamlibdir=/usr/lib/security \
	-Dpam=enabled \
	-Dpolkit=enabled \
	-Dacl=enabled \
	-Dutmp=false \
	-Daudit=disabled \
	-Dselinux=disabled \
	-Dsmack=false \
	-Dxenctrl=disabled \
	-Ddbus=disabled \
	-Dtranslations=false \
	-Dman=enabled \
	-Dhtml=disabled \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

rm "$PKG/usr/share/dbus-1/system-services/org.freedesktop.login1.service"
rmdir "$PKG/usr/share/dbus-1/system-services"
