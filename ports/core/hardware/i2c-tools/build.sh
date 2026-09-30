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

# BUILD_DYNAMIC_LIB puts libi2c.so where the eeprom tools link against it;
# without USE_STATIC_LIB=0 they are linked against the archive instead and the
# shared library ships with nothing using it.
make PREFIX=/usr BUILD_DYNAMIC_LIB=1 USE_STATIC_LIB=0
make PREFIX=/usr BUILD_DYNAMIC_LIB=1 USE_STATIC_LIB=0 DESTDIR=$PKG install
