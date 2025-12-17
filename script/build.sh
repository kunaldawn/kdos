#!/bin/bash
set -e
source script/env.sh

# If running in interactive terminal and python3 exists, use TUI
if [ -t 1 ] && command -v python3 &> /dev/null; then
    exec python3 script/build_tui.py
else
    # Fallback / CI Mode
    echo ">>> Running in Non-Interactive / Legacy Mode"
    bash -e script/00_verify.sh
    bash -e script/01_cross_toolchain.sh
    bash -e script/02_target_base.sh
    bash -e script/03_target_libs.sh
    bash -e script/04_target_tools.sh
    bash -e script/05_native_toolchain.sh
    bash -e script/06_kernel.sh
    bash -e script/07_package.sh
    echo ">>> Build pipeline finished."
fi
