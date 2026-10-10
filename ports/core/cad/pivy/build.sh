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

# SWIG 4.4 generates a multi-phase module init whose exec function returns
# int; soqt.i's %init still returns NULL on failure, where coin.i already
# switches on SWIG_VERSION. The patch gives soqt.i the same switch, returning
# -1, the failure value of an exec function.
patch -p1 -i "$PORT_SRC/swig-4.4-init.patch"

# SWIG 4.3 dropped the Python 3 spellings it had defined for the Python 2
# PyInt_* and PyString_* calls, and pivy's typemaps still make them outside
# their PY_2 branches. The flags restore SWIG 4.2's own mappings for the
# names pivy uses.
export CXXFLAGS="$CXXFLAGS -DPyInt_AsLong=PyLong_AsLong \
	-DPyInt_FromLong=PyLong_FromLong -DPyString_Check=PyBytes_Check \
	-DPyString_FromString=PyUnicode_FromString \
	-DPyString_AsString=PyBytes_AsString -DPyString_Size=PyBytes_Size"

# PIVY_USE_QT6 must match the Qt SoQt was built with: SoQt's config is found
# first, and pivy then requires that Qt's widgets for pivy.gui.soqt.
mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DPIVY_USE_QT6=ON \
	-DDISABLE_SWIG_WARNINGS=ON \
	-DPython_EXECUTABLE=/usr/bin/python3
ninja
DESTDIR=$PKG ninja install
