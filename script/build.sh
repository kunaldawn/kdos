#!/bin/bash
set -e
source script/env.sh

# Dynamic CLI Runner
# Finds all top-level runner scripts following the XX_ naming convention
# and executes them. This preserves the "Runner" logic for CLI users

# Find scripts at depth 2 (script/Category/Script.sh) matching pattern pattern
RUNNERS=$(find script -mindepth 2 -maxdepth 2 -name "[0-9][0-9]_*.sh" | sort)

if [ -t 1 ] && command -v python3 &> /dev/null; then
    exec python3 script/build_tui.py
else
    echo ">>> KDOS Build System (Dynamic CLI)"
    
    for runner in $RUNNERS; do
        echo "================================================================================"
        echo ">>> Executing Module: $runner"
        echo "================================================================================"
        bash -e "$runner"
    done
    
    echo ">>> Build pipeline finished."
fi
