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
# its own copy of the sip runtime, as upstream's does.
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
TOML
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

ls "$PKG"/usr/lib/python3*/site-packages/pynest2d*.so >/dev/null
