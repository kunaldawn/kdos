#!/bin/bash
# Title: Filesystem Skeleton
set -e
source script/env.sh

echo ">>> Preparing Target Filesystem..."

# Define FHS structure
DIRS=(
    "bin" "sbin" "lib" "usr/bin" "usr/sbin" "usr/lib" "usr/include"
    "dev" "proc" "sys" "tmp" "run" "var/log" "var/tmp" "root" "home" "etc/ssl/certs"
)

for dir in "${DIRS[@]}"; do
    mkdir -p "$SYSROOT/$dir"
done

# Permissions
chmod 1777 "$SYSROOT/tmp"
chmod 1777 "$SYSROOT/var/tmp"
chmod 0750 "$SYSROOT/root"

# Copy Skeleton if it exists (for repo-controlled configs)
if [ -d "$WORKSPACE/fs" ]; then
    cp -rv $WORKSPACE/fs/* $SYSROOT/
    
    # Ensure init scripts are executable
    if [ -f "$SYSROOT/etc/init.d/rcS" ]; then
        chmod +x "$SYSROOT/etc/init.d/rcS"
    fi
fi

echo ">>> Filesystem skeleton ready."
