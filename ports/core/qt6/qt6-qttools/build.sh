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

# qdoc and the clang-based lupdate parser are off: both need libclang at build
# time for documentation this tree does not build, and lupdate keeps its own C++
# parser. Assistant stores its help collections in SQLite through Qt SQL, and
# renders them with the litehtml copy qttools carries. No desktop entry is
# written: Designer, Linguist and Assistant are development tools, reached as
# designer6, linguist6 and assistant6.
mkdir -p build && cd build
cmake .. -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DQT_BUILD_EXAMPLES=OFF \
	-DQT_BUILD_TESTS=OFF \
	-DFEATURE_clang=OFF \
	-DFEATURE_qdoc=OFF \
	-DFEATURE_assistant=ON \
	-DFEATURE_designer=ON \
	-DFEATURE_linguist=ON \
	-DFEATURE_qdbus=ON \
	-DFEATURE_qtdiag=ON \
	-DFEATURE_pixeltool=ON \
	-DFEATURE_qtplugininfo=ON \
	-DFEATURE_distancefieldgenerator=ON
ninja
DESTDIR=$PKG ninja install
