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

# The rule is installed by hand to /usr/lib/udev/rules.d: meson would take the
# directory from eudev's udev.pc, which names /lib/udev.
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
