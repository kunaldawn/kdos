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

# DFU IS THE ONE FLASHING ROUTE THAT NEEDS NO SECOND PIECE OF HARDWARE. An
# STM32, an ESP32-S3 or a keyboard controller held in bootloader mode presents
# as a plain USB device and takes its own firmware over the standard Device
# Firmware Upgrade class — no JTAG probe, no serial adapter, one cable. That is
# what makes it the recovery path when the debug header is not populated.
./autogen.sh
./configure --prefix=/usr
make
make DESTDIR=$PKG install
