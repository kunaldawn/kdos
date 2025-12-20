#!/bin/bash
# Title: Cross Toolchain
set -e
source script/env.sh

echo ">>> Building Cross-Toolchain ($TARGET)..."

# Run sub-scripts in order
for script in script/01_toolchain/01_cross/*.sh; do
    if [ -f "$script" ]; then
        bash -e "$script"
    fi
done

echo ">>> Cross-Toolchain Ready."
