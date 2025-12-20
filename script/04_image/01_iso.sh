#!/bin/bash
# Title: ISO Packaging
set -e
source script/env.sh

echo ">>> Packaging KDOS..."

for script in script/04_image/01_iso/*.sh; do
    if [ -f "$script" ]; then
        bash -e "$script"
    fi
done
