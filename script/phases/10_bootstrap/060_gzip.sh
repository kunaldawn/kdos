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
source script/phases/10_bootstrap/phase.env
source script/lib/port.sh

if [ -f "$MARK/gzip" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

echo ">>> Building gzip..."

# Unpack the gzip port's source tarball
GZIP_SRC=$(extract_port_source gzip)

cd "$GZIP_SRC"

./configure \
    --host=$KDOS_TARGET \
    --build=$(build-aux/config.guess) \
    --prefix=/usr
make
make DESTDIR=$SYSROOT install

rm -rf "$GZIP_SRC"
touch "$MARK/gzip"
