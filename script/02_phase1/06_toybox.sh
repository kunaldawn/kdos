#!/bin/bash
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
cat >> .config <<EOF
CONFIG_EXPR=y
CONFIG_GETTY=y
CONFIG_INIT=y
CONFIG_TR=y
CONFIG_AWK=y
EOF
CC=$KDOS_TARGET-gcc make PREFIX=$SYSROOT install -j1

rm -rf "$TOYBOX_SRC"
touch "$MARK/toybox"

