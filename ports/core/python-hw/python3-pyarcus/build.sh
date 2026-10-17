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

_pblibs=
for _l in $(pkg-config --libs-only-l protobuf); do
	_pblibs="$_pblibs${_pblibs:+, }\"${_l#-l}\""
done

cat > pyproject.toml <<TOML
[build-system]
requires = ["sip >=6.8"]
build-backend = "sipbuild.api"

[project]
name = "pyArcus"
version = "$version"

[tool.sip.project]
sip-files-dir = "python"

[tool.sip.bindings.pyArcus]
exceptions = true
release-gil = true
pep484-pyi = true
headers = ["pyArcus/PythonMessage.h"]
sources = ["src/PythonMessage.cpp"]
include-dirs = ["$SRC/include"]
libraries = ["Arcus", $_pblibs]
extra-compile-args = ["-std=c++17"]
TOML
pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

ls "$PKG"/usr/lib/python3*/site-packages/pyArcus*.so >/dev/null
