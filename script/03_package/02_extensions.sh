#!/bin/bash
# Title: Extensions
set -e
source script/env.sh

echo ">>> Building Extensions..."

# Define extension groups in order
# Define extension groups in order
# Subdirectories are now 01_libs, 02_sys, etc.
groups=("01_libs" "02_sys" "03_net" "04_utils" "05_editors" "06_tui" "07_dev")

for group in "${groups[@]}"; do
    echo ">>> Processing Group: $group"
    for script in script/03_package/02_extensions/$group/*.sh; do
        if [ -f "$script" ]; then
            bash -e "$script"
        fi
    done
done

echo ">>> Extensions Built."
