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

patch -p1 -i "$PORT_SRC/melt-include-missing-system-headers.patch"
patch -p1 -i "$PORT_SRC/mlt-man-link-destdir.patch"

mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_DISABLE_FIND_PACKAGE_JACK=ON \
	-DGPL=ON \
	-DGPL3=ON \
	-DBUILD_TESTING=OFF \
	-DBUILD_DOCS=OFF \
	-DCLANG_FORMAT=OFF \
	-DMOD_AVFORMAT=ON \
	-DUSE_AVDEVICE=ON \
	-DMOD_DECKLINK=OFF \
	-DMOD_FREI0R=ON \
	-DMOD_GDK=ON \
	-DMOD_GLAXNIMATE_QT6=ON \
	-DMOD_JACKRACK=ON \
	-DUSE_LV2=ON \
	-DUSE_VST2=ON \
	-DMOD_KDENLIVE=ON \
	-DMOD_MOVIT=ON \
	-DMOD_NDI=OFF \
	-DMOD_NORMALIZE=ON \
	-DMOD_OLDFILM=ON \
	-DMOD_OPENCV=ON \
	-DMOD_OPENFX=ON \
	-DMOD_PLUS=ON \
	-DMOD_PLUSGPL=ON \
	-DMOD_QT6=ON \
	-DMOD_RESAMPLE=ON \
	-DMOD_RTAUDIO=ON \
	-DMOD_RUBBERBAND=ON \
	-DMOD_RNNOISE=ON \
	-DMOD_SDL1=OFF \
	-DMOD_SDL2=ON \
	-DMOD_SOX=ON \
	-DMOD_SPATIALAUDIO=ON \
	-DMOD_VIDSTAB=ON \
	-DMOD_VORBIS=ON \
	-DMOD_XINE=ON \
	-DMOD_XML=ON \
	-DSWIG_PYTHON=OFF
ninja
DESTDIR=$PKG ninja install
test -L "$PKG/usr/share/man/man1/melt.1"
