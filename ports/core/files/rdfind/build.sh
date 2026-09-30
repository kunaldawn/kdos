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

# Autoconf 2.72's C++11 probe fails under the compiler's C++20 default and
# answers by adding -std=gnu++11 to CXX, below the C++17 this code asserts.
# The empty cache value records that no option is needed.
./configure --prefix=/usr --mandir=/usr/share/man --with-xxhash \
	ac_cv_prog_cxx_cxx11=
make
make DESTDIR=$PKG install
