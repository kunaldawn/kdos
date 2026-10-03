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


# protobuf's headers call into abseil, so the module links everything
# protobuf's pkg-config names rather than libprotobuf alone.
_pblibs=
for _l in $(pkg-config --libs-only-l protobuf); do
	_pblibs="$_pblibs${_pblibs:+, }\"${_l#-l}\""
done

# The upstream build writes pyproject.toml through Conan's toolchain and
# compiles through CMake; sip's own backend builds the same module from the
# same .sip files, with the settings that toolchain writes: exceptions,
# released GIL and a .pyi stub. No sip-module is named, so the module carries
# its own copy of the sip runtime, as upstream's does. sip compiles as C++11
# unless told otherwise, and PythonMessage.h (std::optional) and abseil, which
# protobuf's headers pull in, need C++17; a -std given in extra-compile-args
# comes after sip's own and wins.
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
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

ls "$PKG"/usr/lib/python3*/site-packages/pyArcus*.so >/dev/null
