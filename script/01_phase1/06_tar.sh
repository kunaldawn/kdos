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

if [ -f "$MARK/tar" ]; then
    exit 0
fi

echo ">>> Building tar..."

# Extract tar and dependencies from ports
TAR_SRC=$(extract_port_source tar)

cd "$TAR_SRC"

FORCE_UNSAFE_CONFIGURE=1 ./configure --prefix=/usr --disable-nls
make
make DESTDIR=$SYSROOT install

rm -rf "$TAR_SRC"
touch "$MARK/tar"
