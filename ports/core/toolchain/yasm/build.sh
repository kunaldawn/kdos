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

# PYTHON IS REQUIRED, NOT OPTIONAL: the tag archive carries no pregenerated
# x86insns.c, so modules/arch/x86/gen_x86_insn.py must run, and
# --enable-python makes a missing interpreter stop configure instead of the
# build. XMLTO=: keeps the prebuilt man pages in the archive: with xmlto found,
# they are regenerated and need the docbook catalog. NLS is off because the
# archive ships no translations.
./autogen.sh --prefix=/usr ac_cv_header_stdc=yes \
	--enable-python \
	--disable-python-bindings \
	--disable-nls \
	XMLTO=:
make -j1
make -j1 DESTDIR=$PKG install
