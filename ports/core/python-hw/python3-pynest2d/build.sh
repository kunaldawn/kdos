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

cat > pyproject.toml <<TOML
[build-system]
requires = ["sip >=6.8"]
build-backend = "sipbuild.api"

[project]
name = "pynest2d"
version = "$version"

[tool.sip.project]
sip-files-dir = "python"

[tool.sip.bindings.pynest2d]
exceptions = true
release-gil = true
pep484-pyi = true
define-macros = ["LIBNEST2D_GEOMETRIES_clipper", "LIBNEST2D_OPTIMIZER_nlopt", "LIBNEST2D_THREADING_std"]
libraries = ["polyclipping", "nlopt"]
extra-compile-args = ["-std=c++17"]
TOML
pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

ls "$PKG"/usr/lib/python3*/site-packages/pynest2d*.so >/dev/null
