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

# TCL VENDORS THIS AND RENAMES EVERY SYMBOL. Built with its own copy, tcl9's
# tclTomMath.h maps `mp_to_unsigned_bin` onto `TclBN_mp_to_unsigned_bin`, which
# is reachable only through tcl's stub table — so yosys, which calls the plain
# names against tcl's public headers, fails to compile against a tcl that
# looks entirely healthy. Building tcl --with-system-libtommath puts
# TCL_WITH_EXTERNAL_TOMMATH in tcl.pc, which is exactly what yosys's CMakeLists
# tests for before importing this library alongside it.
make -f makefile.shared PREFIX=/usr LIBPATH=/usr/lib
make -f makefile.shared PREFIX=/usr LIBPATH=/usr/lib DESTDIR=$PKG install
