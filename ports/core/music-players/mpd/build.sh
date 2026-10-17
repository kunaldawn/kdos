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

meson setup build \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=lib \
	--buildtype=release \
	--wrap-mode=nofallback \
	-Dalsa=enabled \
	-Dpipewire=enabled \
	-Dpulse=disabled \
	-Djack=enabled \
	-Dsndio=disabled \
	-Dao=disabled \
	-Dopenal=disabled \
	-Doss=disabled \
	-Dshout=disabled \
	-Dsolaris_output=disabled \
	-Dhttpd=false \
	-Dsnapcast=false \
	-Dnlohmann_json=disabled \
	-Dffmpeg=enabled \
	-Dflac=enabled \
	-Dvorbis=enabled \
	-Dopus=enabled \
	-Dsndfile=enabled \
	-Dmpg123=enabled \
	-Did3tag=enabled \
	-Dmikmod=enabled \
	-Dfluidsynth=enabled \
	-Dmad=disabled \
	-Dfaad=disabled \
	-Dwavpack=disabled \
	-Dadplug=disabled \
	-Daudiofile=disabled \
	-Dgme=disabled \
	-Dmodplug=disabled \
	-Dopenmpt=disabled \
	-Dmpcdec=disabled \
	-Dsidplay=disabled \
	-Dtremor=disabled \
	-Dwildmidi=disabled \
	-Dchromaprint=enabled \
	-Dlame=enabled \
	-Dvorbisenc=enabled \
	-Dtwolame=enabled \
	-Dshine=disabled \
	-Dsoxr=enabled \
	-Dlibsamplerate=enabled \
	-Dsqlite=enabled \
	-Dicu=enabled \
	-Diconv=disabled \
	-Dpcre=enabled \
	-Dexpat=enabled \
	-Dzlib=enabled \
	-Dbzip2=disabled \
	-Dzzip=disabled \
	-Diso9660=enabled \
	-Dcdio_paranoia=enabled \
	-Dcurl=enabled \
	-Dmms=disabled \
	-Dnfs=disabled \
	-Dsmbclient=disabled \
	-Dwebdav=disabled \
	-Dqobuz=disabled \
	-Dupnp=disabled \
	-Dlibmpdclient=disabled \
	-Dudisks=disabled \
	-Ddbus=disabled \
	-Dzeroconf=disabled \
	-Dio_uring=enabled \
	-Dsyslog=enabled \
	-Dipv6=enabled \
	-Dsystemd=disabled \
	-Ddocumentation=enabled \
	-Dhtml_manual=false \
	-Dmanpages=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
