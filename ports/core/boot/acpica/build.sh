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

# iasl is what edk2 and the qemu firmware builds compile their ACPI tables
# with. acpiexamples is upstream's sample of embedding the interpreter in a
# program and is not a tool, so it is not installed. NOWERROR because the
# makefile adds -Werror to its own warning set, and any warning a newer gcc
# adds to that set would stop the build.
make NOWERROR=TRUE
make NOWERROR=TRUE DESTDIR=$PKG install
rm -f "$PKG/usr/bin/acpiexamples"
