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


# libnest2d is header-only: the geometry, optimiser and threading backends are
# chosen by the three macros its CMake target would have exported, and the
# module links the two libraries behind them.

# The upstream build writes pyproject.toml through Conan's toolchain and
# compiles through CMake; sip's own backend builds the same module from the
# same .sip files, with the settings that toolchain writes: exceptions,
# released GIL and a .pyi stub. No sip-module is named, so the module carries
# its own copy of the sip runtime, as upstream's does. sip compiles as C++11
# unless told otherwise, and Boost.Geometry, which libnest2d's clipper backend
# includes, needs C++14 or later; libnest2d itself is C++17. A -std given in
# extra-compile-args comes after sip's own and wins.
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
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

ls "$PKG"/usr/lib/python3*/site-packages/pynest2d*.so >/dev/null
