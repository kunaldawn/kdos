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

patch -p1 -i "$PORT_SRC/no-update-check.patch"
patch -p1 -i "$PORT_SRC/ffmpeg-mathops-shift.patch"
patch -p1 -i "$PORT_SRC/musl-glibc-calls.patch"
patch -p1 -i "$PORT_SRC/qt6-moc-headers-only.patch"
patch -p1 -i "$PORT_SRC/qt6-no-cxx11.patch"
patch -p1 -i "$PORT_SRC/mp4-menu-sentinel.patch"
patch -p1 -i "$PORT_SRC/x265-stat-filename.patch"
patch -p1 -i "$PORT_SRC/fluxsmooth-table-size.patch"

_features="-DQT6=ON -DOPENGL=ON -DSDL=OFF
	-DALSA=ON -DPULSEAUDIO=ON -DJACK=OFF -DARTS=OFF
	-DESD=OFF -DOSS=OFF -DVDPAU=OFF -DNVENC=OFF -DXVIDEO=OFF
	-DLIBVA=OFF -DVAPOURSYNTH=OFF -DX264=ON -DX265=ON
	-DVPXENC=ON -DAOMDEC=OFF -DOPUS=ON -DOPUS_ENCODER=ON -DLAME=ON
	-DVORBIS=ON -DLIBVORBIS=ON -DFAAD=ON -DFDK_AAC=OFF -DFAAC=OFF
	-DTWOLAME=ON -DAFTEN=OFF -DDCAENC=OFF -DLIBDCA=OFF -DXVID=OFF
	-DOPENCORE_AMRNB=OFF -DOPENCORE_AMRWB=OFF -DFREETYPE2=ON
	-DFONTCONFIG=ON -DFRIBIDI=ON -DGETTEXT=OFF
	-DTINYPY=ON -DUSE_EXTERNAL_LIBASS=ON -DUSE_EXTERNAL_LIBMAD=ON"

_part() {
	cmake -S "$2" -B "$1" -G "Unix Makefiles" \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DFAKEROOT="$PKG" \
		-DENABLE_QT6=True \
		-DLRELEASE_EXECUTABLE=/usr/lib/qt6/bin/lrelease \
		$_features "${@:3}" \
		-Wno-dev
	make -C "$1"
	make -C "$1" DESTDIR="$PKG" install
}

_part buildCore avidemux_core
_part buildQt6 avidemux/qt4
_part buildCli avidemux/cli
_part buildPluginsCommon avidemux_plugins -DPLUGIN_UI=COMMON
_part buildPluginsQt6 avidemux_plugins -DPLUGIN_UI=QT4
_part buildPluginsCLI avidemux_plugins -DPLUGIN_UI=CLI
_part buildPluginsSettings avidemux_plugins -DPLUGIN_UI=SETTINGS

rm -rf "$PKG/usr/share/avidemux6/qt6/i18n"

cat > "$PKG/usr/share/applications/org.avidemux.Avidemux.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Avidemux
GenericName=Video Cutter
Comment=Cut, filter and re-encode video files
Exec=avidemux3_qt6 %f
Icon=org.avidemux.Avidemux
Terminal=false
StartupWMClass=avidemux3_qt6
Categories=AudioVideo;AudioVideoEditing;Video;
Keywords=video;cut;trim;encode;filter;avidemux;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.avidemux.Avidemux.desktop"
