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

# THE ENGINE WITHOUT A LANGUAGE FILE DOES NOTHING, AND THAT IS THE TRAP. A
# tesseract install with no `.traineddata` starts, accepts an image and exits
# with "Failed loading language 'eng'" — which reads as a broken build. The
# data is a separate download upstream and on this distro that means it has to
# be a `source =` line, which it is: `tessdata_fast`'s English model, ~4 MB.
#
# fast rather than `tessdata_best`: best is four times the size for a few
# points of accuracy on clean scans, and on a stick the size is the point.
# Other languages are files somebody drops into /usr/share/tessdata later.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TRAINING_TOOLS=OFF \
	-DDISABLE_TIFF=OFF \
	-DGRAPHICS_DISABLED=ON \
	-DUSE_SYSTEM_ICU=ON
ninja
DESTDIR=$PKG ninja install

# A non-archive source is not unpacked into $SRC_ROOT — it stays where kpkg
# fetched it, which is the port directory.
install -Dm644 $PORT_SRC/tessdata-eng-4.1.0.traineddata $PKG/usr/share/tessdata/eng.traineddata
