# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# THE CHEAP ANALYSERS ARE BLANK UNTIL THIS IS UPLOADED. An FX2 board — the
# 24 MHz, 8-channel Saleae clones and their relatives — enumerates with no
# firmware at all; libsigrok's fx2lafw driver loads fx2lafw-*.fw from
# /usr/share/sigrok-firmware into its RAM on every plug-in, and without the
# file the device is listed by a scan and fails to open.
#
# It is 8051 code, compiled from source by the sdcc port rather than taken as
# a prebuilt binary: sdcc's mcs51 back end and device libraries are the whole
# toolchain it needs. sdcc-syntax.patch is what lets that sdcc read it: the
# release spells `__at` addresses and interrupt numbers in a form sdcc 4.2.3
# stopped accepting, and the build otherwise stops at the first register in
# fx2regs.h.
patch -p1 -i "$PORT_SRC/sdcc-syntax.patch"
./configure --prefix=/usr
make
make DESTDIR=$PKG install
