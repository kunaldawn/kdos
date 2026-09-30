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

# The Qt 6 half only, matching the audacious port; every GTK-only plugin
# (ladspa, aosd, the GTK UI and skins) goes with -Dgtk=false. Output is
# PipeWire, with ALSA as the fallback; pulse, SDL, Qt Multimedia, OSS and
# sndio would be further routes to the same sound server, and JACK is not on
# this system. Every plugin is named so the set is explicit, but a plugin
# switched on whose library is missing is still dropped, with only a line in
# meson's summary; that summary is where a missing dependency shows.
#
# The SID plugin needs libsidplayfp built against libresidfp; without it
# meson drops the plugin with a warning. Off because they only work online:
# scrobbler2 (Last.fm), lyrics (lyrics sites), streamtuner (radio
# directories), ampache, CDDB lookups, and the neon and mms stream
# transports. The X11 global hotkey plugin grabs keys on an X server, which a
# Wayland session never gives it.
meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release -Db_ndebug=if-release \
	-Dqt=true \
	-Dqt5=false \
	-Dgtk=false \
	-Dgtk2=false \
	-Dqtui=true \
	-Dskins=true \
	-Dasx=true \
	-Dasx3=true \
	-Dm3u=true \
	-Dpls=true \
	-Dxspf=true \
	-Dgio=true \
	-Daac=true \
	-Damidiplug=true \
	-Dcdaudio=true \
	-Dconsole=true \
	-Dffaudio=true \
	-Dflac=true \
	-Dmetronome=true \
	-Dmodplug=true \
	-Dmpg123=true \
	-Dopus=true \
	-Dpsf=true \
	-Dsndfile=true \
	-Dtonegen=true \
	-Dvorbis=true \
	-Dvtx=true \
	-Dwavpack=true \
	-Dxsf=true \
	-Dalsa=true \
	-Dpipewire=true \
	-Dfilewriter=true \
	-Dfilewriter-flac=true \
	-Dfilewriter-mp3=true \
	-Dfilewriter-ogg=true \
	-Dalbumart=true \
	-Ddelete-files=true \
	-Dfilebrowser=true \
	-Dmpris2=true \
	-Dnotify=true \
	-Dplayback-history=true \
	-Dplaylist-manager=true \
	-Dsearchtool=true \
	-Dsongchange=true \
	-Dsonginfo=true \
	-Dstatusicon=true \
	-Dbackground-music=true \
	-Dbitcrusher=true \
	-Dcompressor=true \
	-Dcrossfade=true \
	-Dcrystalizer=true \
	-Decho=true \
	-Dmixer=true \
	-Dresample=true \
	-Dsilence-removal=true \
	-Dsoxr=true \
	-Dspeedpitch=true \
	-Dstereo=true \
	-Dvoice-removal=true \
	-Dblurscope=true \
	-Dgl-spectrum=true \
	-Dspectrum-analyzer=true \
	-Dvumeter=true \
	-Dscrobbler2=false \
	-Dlyrics=false \
	-Dstreamtuner=false \
	-Dampache=false \
	-Dcdaudio-cddb=false \
	-Dneon=false \
	-Dmms=false \
	-Dcue=true \
	-Dsid=true \
	-Dopenmpt=true \
	-Dadplug=true \
	-Dbs2b=true \
	-Dlirc=true \
	-Dhotkey=false \
	-Dladspa=false \
	-Daosd=false \
	-Dgtkui=false \
	-Dpulse=false \
	-Dsdlout=false \
	-Dqtaudio=false \
	-Doss=false \
	-Dsndio=false \
	-Djack=false \
	-Dcoreaudio=false \
	-Dmac-now-playing=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Bundled data is English only; Qt falls back to the source strings.
rm -rf "$PKG/usr/share/locale"
