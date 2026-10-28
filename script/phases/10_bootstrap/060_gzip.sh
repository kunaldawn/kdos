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

# configure installs zless only when it finds less on $PATH. The container
# has one and the gzip port's chroot does not, so the port's package carries
# no zless: this one would be owned by nothing and never replaced.
rm -f $SYSROOT/usr/bin/zless $SYSROOT/usr/share/man/man1/zless.1

rm -rf "$GZIP_SRC"
touch "$MARK/gzip"
