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

if [ -f "$MARK/coreutils" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

echo ">>> Building coreutils..."

COREUTILS_SRC=$(extract_port_source coreutils)

cd "$COREUTILS_SRC"

FORCE_UNSAFE_CONFIGURE=1 fu_cv_sys_stat_statvfs=n ./configure \
    --host=$KDOS_TARGET \
    --build=$(build-aux/config.guess) \
    --prefix=/usr \
    --disable-nls \
    --without-openssl \
    --without-selinux \
    --without-libgmp \
    --disable-acl \
    --disable-xattr \
    --disable-libcap \
    --enable-install-program=arch,hostname

make
make DESTDIR=$SYSROOT install

rm -rf "$COREUTILS_SRC"
touch "$MARK/coreutils"
