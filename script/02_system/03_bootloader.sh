#!/bin/bash
# Title: Bootloaders
set -e
source script/env.sh

echo ">>> Building Bootloaders..."

for script in script/02_system/03_bootloader/*.sh; do
    if [ -f "$script" ]; then
        bash -e "$script"
    fi
done

echo ">>> Bootloaders Built."
