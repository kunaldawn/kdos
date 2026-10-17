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

cd src
export QMAKE='qmake6 QMAKE_CFLAGS+=$$(CFLAGS) QMAKE_CXXFLAGS+=$$(CXXFLAGS) QMAKE_LFLAGS+=$$(LDFLAGS) QMAKE_LFLAGS_RPATH='
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dqtgui=true -Dwebkit=false -Dwebengine=false -Dwebpreview=false \
	-Dpython-chm=false -Dpython-aspell=true -Daspell=true \
	-Dx11mon=false -Dsystemd=false -Dlibmagic=true \
	-Drecollq=true -Dindexer=true -Drclgrep=true -Dext4-birthtime=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
