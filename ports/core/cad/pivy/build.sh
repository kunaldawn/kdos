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

patch -p1 -i "$PORT_SRC/swig-4.4-init.patch"

export CXXFLAGS="$CXXFLAGS -DPyInt_AsLong=PyLong_AsLong \
	-DPyInt_FromLong=PyLong_FromLong -DPyString_Check=PyBytes_Check \
	-DPyString_FromString=PyUnicode_FromString \
	-DPyString_AsString=PyBytes_AsString -DPyString_Size=PyBytes_Size"

mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DPIVY_USE_QT6=ON \
	-DDISABLE_SWIG_WARNINGS=ON \
	-DPython_EXECUTABLE=/usr/bin/python3
ninja
DESTDIR=$PKG ninja install
