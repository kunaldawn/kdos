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

# THE DRIVE DATABASE IS IN THE SOURCE AND MUST STAY THERE. smartctl reads
# vendor attributes through a table of known models; `update-smart-drivedb`
# fetches a newer one over the network, which on this distro is a promise that
# cannot be kept, so the compiled-in copy is the whole database this machine
# will ever have. --without-update-smart-drivedb removes the script rather than
# shipping one that fails.
#
# smartd is not supervised by anything here: this is the diagnostic half, and
# `kdos doctor`'s Hardware section is what asks the question on a schedule.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--without-update-smart-drivedb \
	--without-systemdsystemunitdir \
	--with-initscriptdir=no \
	--with-nvme-devicescan
make
make DESTDIR=$PKG install
