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


# THE HIDRAW BACKEND, NOT libusb. A security key is a HID device and hidraw is
# the path that needs no unbinding of the kernel driver; the libusb backend
# claims the interface and a key already opened by anything else is then
# invisible.
#
# udev RULES ARE THE OTHER HALF, AND UPSTREAM'S ARE NOT INSTALLED. Without a
# grant the hidraw node is root-only and `ssh-keygen -t ed25519-sk` fails as a
# permission error that reads like a missing key. udev/70-u2f.rules grants a
# `plugdev` group this system does not have — eudev resolves an unknown group
# to gid 0 — and a `uaccess` tag nothing consumes, and it sorts after
# 70-kdos-fido.rules, so installing it would take back the grant that file
# makes. fs/etc/udev/rules.d/70-kdos-fido.rules is the grant: it keys on the
# ID_SECURITY_TOKEN that eudev's fido_id sets from the HID report descriptor,
# so it covers every FIDO key rather than a vendor table.
#
# PC/SC IS THE SMARTCARD-READER PATH, through pcscd; without it a key behind a
# CCID reader is not listed. MANDOC_PATH is pinned OFF: with mandoc on PATH
# the build adds an HTML copy of every page, so the package would follow build
# order. A *-NOTFOUND value does not pin it, because find_program searches
# again for a cached NOTFOUND.
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF \
	-DBUILD_EXAMPLES=OFF -DBUILD_TESTS=OFF -DBUILD_MANPAGES=ON \
	-DUSE_HIDAPI=OFF -DNFC_LINUX=ON -DUSE_PCSC=ON \
	-DMANDOC_PATH=OFF
make
make DESTDIR=$PKG install
