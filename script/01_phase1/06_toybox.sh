#!/bin/bash

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#    KDOS – forged by hand.
#    KD's Homebrew OS
# ---------------------------------

set -e
source script/phase1.env.sh
source script/util/port.sh

if [ -f "$MARK/toybox" ]; then
    exit 0
fi

echo ">>> Building toybox..."

# Extract toybox and dependencies from ports
TOYBOX_SRC=$(extract_port_source toybox)

cd "$TOYBOX_SRC"

make defconfig -j1
sed -i 's/# CONFIG_EXPR is not set/CONFIG_EXPR=y/' .config
sed -i 's/# CONFIG_GETTY is not set/CONFIG_GETTY=y/' .config
sed -i 's/# CONFIG_INIT is not set/CONFIG_INIT=y/' .config
sed -i 's/# CONFIG_TR is not set/CONFIG_TR=y/' .config
sed -i 's/# CONFIG_AWK is not set/CONFIG_AWK=y/' .config
sed -i 's/# CONFIG_MDEV is not set/CONFIG_MDEV=y/' .config
CC=$KDOS_TARGET-gcc make PREFIX=$SYSROOT install -j1

rm -rf "$TOYBOX_SRC"
touch "$MARK/toybox"

