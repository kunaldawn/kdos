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

LDFLAGS="$LDFLAGS -lintl" meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --localstatedir=/var \
	-Dbuildtype=release \
	-Ddaemon=false \
	-Dclient=true \
	-Ddoxygen=false \
	-Dman=true \
	-Dtests=false \
	-Dgcov=false \
	-Dipv6=true \
	-Ddbus=enabled \
	-Dglib=enabled \
	-Dasyncns=disabled \
	-Dgtk=disabled \
	-Dx11=disabled \
	-Dsystemd=disabled \
	-Doss-output=disabled \
	-Dfftw=disabled \
	-Dvalgrind=disabled \
	-Dalsa=disabled \
	-Davahi=disabled \
	-Dbluez5=disabled \
	-Dbluez5-gstreamer=disabled \
	-Dconsolekit=disabled \
	-Delogind=disabled \
	-Dgsettings=disabled \
	-Dgstreamer=disabled \
	-Djack=disabled \
	-Dlirc=disabled \
	-Dopenssl=disabled \
	-Dorc=disabled \
	-Dsamplerate=disabled \
	-Dsoxr=disabled \
	-Dspeex=disabled \
	-Dtcpwrap=disabled \
	-Dudev=disabled \
	-Dwebrtc-aec=disabled \
	-Dbashcompletiondir=/usr/share/bash-completion/completions \
	-Dzshcompletiondir=/usr/share/zsh/site-functions
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
