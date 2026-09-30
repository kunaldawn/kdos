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
name = "pySavitar"
version = "$version"

[tool.sip.project]
sip-files-dir = "python"

[tool.sip.bindings.pySavitar]
exceptions = true
release-gil = true
pep484-pyi = true
libraries = ["Savitar", "pugixml"]
TOML
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

ls "$PKG"/usr/lib/python3*/site-packages/pySavitar*.so >/dev/null
