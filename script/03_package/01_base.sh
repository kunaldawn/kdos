#!/bin/bash
# Title: Base System
set -e
source script/env.sh

echo ">>> Building Base System..."

# Run Base scripts
for script in script/03_package/01_base/*.sh; do
    if [ -f "$script" ]; then
        bash -e "$script"
    fi
done

echo ">>> Base System Built."
