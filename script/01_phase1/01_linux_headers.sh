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

set -e
source script/phase1.env.sh
source script/util/port.sh

if [ -f "$MARK/linux_headers" ]; then
    exit 0
fi

echo ">>> Installing Linux Headers..."

LINUX_SRC=$(extract_port_source linux)
cd "$LINUX_SRC"
echo "$LINUX_SRC"

make ARCH=x86_64 headers_install INSTALL_HDR_PATH=$SYSROOT/usr

rm -rf "$LINUX_SRC"
touch "$MARK/linux_headers"
