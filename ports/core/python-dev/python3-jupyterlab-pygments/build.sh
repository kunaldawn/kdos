#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The JupyterLab extension half ships built in the release tarball;
# HATCH_JUPYTER_BUILDER_SKIP_NPM keeps the build hook from running npm, and
# its ensured targets still fail the build if a built file is missing.
export HATCH_JUPYTER_BUILDER_SKIP_NPM=1
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
test -e "$PKG"/usr/share/jupyter/labextensions/jupyterlab_pygments/package.json
