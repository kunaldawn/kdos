#!/bin/bash
# Title: Native Toolchain
set -e
source script/env.sh

echo ">>> Building Native Toolchain (Self-Hosting)..."

for script in script/01_toolchain/02_native/*.sh; do
    if [ -f "$script" ]; then
        bash -e "$script"
    fi
done

echo ">>> Native Toolchain Installed."
