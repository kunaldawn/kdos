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

# GMP IS WHAT MAKES THE EXACT SIMPLEX EXACT. Without it glpk falls back to
# floating point everywhere, which for a mixed-integer problem means an answer
# that is optimal-looking and occasionally wrong at the last bit — and there is
# nothing in the output to say which. `--with-gmp` is the whole difference.
#
# It also ships GNU MathProg, which is why this is here beside HiGHS rather
# than replaced by it: a model written as text you can read and diff is worth
# more offline than a faster solver you drive from a library.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--with-gmp
make
make DESTDIR=$PKG install
