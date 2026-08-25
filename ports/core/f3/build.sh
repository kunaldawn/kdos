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

# ON A DISTRO WHOSE MEDIUM IS THE SOFTWARE LIBRARY, A FAKE STICK IS DATA LOSS
# THAT REPORTS SUCCESS. A counterfeit wraps its writes back over the same real
# cells, so every copy completes, every checksum at write time passes, and the
# corpus is unreadable months later. f3write/f3read fill the device and read it
# back; that is the only test that catches it.
make
make DESTDIR=$PKG PREFIX=/usr install

# f3probe, f3brew and f3fix are the extra tools and they need parted and
# libudev. They are built separately upstream because those deps are optional —
# they are not optional here: f3probe is the fast test, and without it the only
# answer takes as long as filling the device.
make extra
make DESTDIR=$PKG PREFIX=/usr install-extra
