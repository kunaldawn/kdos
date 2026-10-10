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

# --without-x, AND OMITTING IT IS NOT THE SAME THING. autoconf's AC_PATH_XTRA
# runs by default, so a configure with no X flag at all LOOKS for X and stops
# on `Couldn't find Xaw library` — it has to be refused out loud. The X plotting
# is ngspice's own Xt window anyway; what a scriptable machine wants is the raw
# file, which gnuplot already draws. --with-readline=yes is what makes the
# interactive prompt usable at all.
#
# --enable-xspice AND --enable-cider are the two that turn this from a netlist
# simulator into the tool the catalogue names: XSPICE is the code-model
# interface every mixed-signal example uses, and CIDER is the device-level
# solver. Both default OFF and neither fails without its dependency.
#
# --with-fftw3=yes still falls back to ngspice's own FFT when fftw3 is missing,
# and libsndfile and libsamplerate (the sndprint command and the voltage
# source's wav input) are unconditional AC_CHECK_LIB probes with no switch, so
# all three are held by `depends` alone.
#
# ONE CONFIGURE BUILDS EITHER THE PROGRAM OR libngspice, never both: KiCad's
# simulator links the shared library, a terminal runs the program. Each is its
# own out-of-tree build with the same features; the library has no prompt,
# so no readline.
_conf="--prefix=/usr --libdir=/usr/lib --disable-static --without-x
	--enable-xspice --enable-cider --enable-openmp --disable-debug
	--with-fftw3=yes"
mkdir build-shared build-program
(cd build-shared && ../configure $_conf --with-ngshared && make && make DESTDIR=$PKG install)
(cd build-program && ../configure $_conf --with-readline=yes && make && make DESTDIR=$PKG install)
