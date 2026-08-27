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


# THE PROTOCOL DECODERS ARE WHY THIS BEATS A SCOPE'S OWN SCREEN. Capturing
# eight channels is the easy half; turning them into "I2C write to 0x48, ACK,
# 0x01" is libsigrokdecode's, and it does about a hundred protocols. On a bench
# with no oscilloscope that can decode, a £15 analyser plus this is the
# instrument.
./configure --prefix=/usr
make
make DESTDIR=$PKG install
