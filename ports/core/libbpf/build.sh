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

cd src

# LIBDIR MUST BE ABSOLUTE. The Makefile's default is `$(PREFIX)/$(LIBSUBDIR)`
# and every install rule concatenates `$(DESTDIR)$(LIBDIR)` WITH NO SEPARATOR —
# so a relative `lib` writes the libraries and the pkg-config file to
# `<pkgdir>lib`, a sibling of the staging tree that is never packaged. The
# package then contains HEADERS AND NOTHING ELSE, installs cleanly, and every
# consumer fails to find a library whose headers are right there. bpftrace is
# the one that noticed, asking for libbpf.a.
#
# LIBSUBDIR is lib64 on x86_64, hence naming LIBDIR rather than letting the
# default stand.
make PREFIX=/usr LIBDIR=/usr/lib
make PREFIX=/usr LIBDIR=/usr/lib DESTDIR=$PKG install
make PREFIX=/usr LIBDIR=/usr/lib DESTDIR=$PKG install_uapi_headers
