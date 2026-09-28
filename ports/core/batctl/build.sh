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


# The kernel module is batman-adv, built as a module in the kernel config;
# this is the tool that adds interfaces to a bat0 mesh and reads its tables.
# The makefile appends its own warnings to CFLAGS and finds libnl-3.0 and
# libnl-genl-3.0 through pkg-config.
make PREFIX=/usr
make PREFIX=/usr DESTDIR=$PKG install
