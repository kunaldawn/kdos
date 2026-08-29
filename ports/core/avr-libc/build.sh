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

# BUILT WITH THE CROSS COMPILER, like picolibc and for the same reason: gcc-avr
# exists first, --without-headers, so libgcc can be built before a libc does.
#
# --host=avr IS THE WHOLE CONFIGURATION. avr-libc's configure refuses to run
# without it, and the check is deliberate — a libc accidentally configured for
# the build machine would compile and install headers that describe the wrong
# architecture entirely.
# CC MUST BE UNSET, and that is a consequence of this tree's own phase env:
# every native phase exports CC=gcc so an autoconf that prefers clang cannot
# silently change toolchain (see potrace). For a CROSS build that pin is the
# wrong answer — avr-libc's configure checks that the compiler is an AVR one
# and stops at "Wrong C compiler found; check the PATH!". --host=avr is what
# should decide it, so the environment has to get out of the way.
unset CC CXX CFLAGS CXXFLAGS LDFLAGS CPPFLAGS

./configure --prefix=/usr --host=avr --build="$(./config.guess)"
make
make DESTDIR=$PKG install
