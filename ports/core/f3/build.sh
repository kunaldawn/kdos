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
# -largp because ARGP IS A GLIBC EXTENSION. f3 parses its options with
# argp_parse and musl does not implement it; the link fails on
# `undefined reference to argp_error` with the object and the symbol name
# interleaved, which reads like a corrupted link line rather than a missing
# library. argp-standalone is the port that provides it.
#
# LDFLAGS, NOT LDLIBS: f3's Makefile adds `-largp` only inside a non-Linux
# branch — on Linux it assumes glibc has it — and every link line ends with
# `$(LDFLAGS) -lm`, so that is the variable the flag has to reach. LDLIBS is
# never referenced.
export LDFLAGS="$LDFLAGS -largp"

make
make DESTDIR=$PKG PREFIX=/usr install

# f3probe, f3brew and f3fix are the extra tools and they need parted and
# libudev. They are built separately upstream because those deps are optional —
# they are not optional here: f3probe is the fast test, and without it the only
# answer takes as long as filling the device.
make extra
make DESTDIR=$PKG PREFIX=/usr install-extra
