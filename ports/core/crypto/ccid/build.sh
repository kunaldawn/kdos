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

patch -p1 -i "$PORT_SRC/no-kobil-switch.patch"

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dpcsclite=true \
	-Dserial=true \
	-Dclass=true \
	-Dcomposite-as-multislot=false \
	-Dzlp=false \
	-Dembedded=false \
	-Dos_log=false \
	-Denable-extras=false \
	-Dudev-rules=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
install -Dm644 src/92_pcscd_ccid.rules "$PKG/usr/lib/udev/rules.d/92_pcscd_ccid.rules"
