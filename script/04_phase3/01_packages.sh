#!/bin/bash
set -e
source script/phase3.env.sh

# Install Dependencies for Packaging and Bootloader
kpkg install -f toybox refind libburn libisofs libisoburn squashfs-tools mtools dosfstools xz zlib
