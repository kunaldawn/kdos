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

# libudev is the hotplug backend; libusb and libudev are exclusive, and libusb
# is ccid's to link, not the daemon's. With polkit on, pcscd refuses every
# client polkit does not grant org.debian.pcsc-lite.access_pcsc and
# access_card, and with no session tracking here only allow_any is ever read —
# `no` for both, so fs/etc/polkit-1/rules.d/50-kdos.rules grants them to wheel
# and a user outside it reaches no reader. The pcscd account is made by
# postinstall.sh.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --localstatedir=/var \
	--buildtype=release \
	-Ddefault_library=shared \
	-Dlibsystemd=false \
	-Dlibudev=true \
	-Dlibusb=false \
	-Dpolkit=true \
	-Dusb=true \
	-Dserial=true \
	-Dfilter_names=true \
	-Dembedded=false \
	-Dusbdropdir=/usr/lib/pcsc/drivers \
	-Dserialconfdir=/etc/reader.conf.d \
	-Dipcdir=/run/pcscd
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Installed whatever the options: nothing here reads units or sysusers.d.
rm -rf "$PKG/usr/lib/systemd" "$PKG/usr/sysusers.d"
install -d "$PKG/usr/lib/pcsc/drivers" "$PKG/etc/reader.conf.d"
