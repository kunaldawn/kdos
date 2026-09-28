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

# DRV_ONLY=1 installs the .drv sources for cups-driverd to generate PPDs from
# on demand. Without it the install copies 265 pre-generated PPDs plus
# translations that are made with recode, which is not a port.
# JBIG stays on (DISABLE_JBIG=0): with it off the colour models are still
# listed in the .drv files and fail at print time.
make DISABLE_JBIG=0 DRV_ONLY=1
make drv DISABLE_JBIG=0 DRV_ONLY=1
make install DISABLE_JBIG=0 DRV_ONLY=1 DESTDIR=$PKG
