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

if [ -f "$MARK/toybox" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
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
sed -i 's/CONFIG_TAR=y/# CONFIG_TAR is not set/' .config
# `file` is the `file` port's, here as in the phase-4 recipe: lesspipe and the
# desktop's type guessing both ask `file -L -s -b --mime`, which the applet has
# no database to answer, and two answers to "what is this file" is one too many.
sed -i 's/CONFIG_FILE=y/# CONFIG_FILE is not set/' .config
CC=$KDOS_TARGET-gcc make PREFIX=$SYSROOT install -j1

rm -rf "$TOYBOX_SRC"
touch "$MARK/toybox"

