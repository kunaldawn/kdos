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

# The update check asks avidemux.org for a newer release at every start, and
# only the preference turns it off; the patch makes that preference default
# to off.
patch -p1 -i "$PORT_SRC/no-update-check.patch"

# Avidemux is built in upstream's bootStrap.bash order, each part against the
# ones installed before it: core (with the FFmpeg 4.4 it carries in
# avidemux_core/ffmpeg_package, patched and linked privately as
# libADM6av*), the Qt 6 GUI, the command-line front end, then the plugins
# once per interface. FAKEROOT is where the later parts look for the core's
# headers and CMake files: $PKG, where each part is installed.
#
# Every feature is named: a missing library otherwise drops its plugin with a
# configure message only. JACK, aRts, ESD and OSS are sound servers this
# system does not run; VDPAU, XvBA, NVENC and Xv are X11 or vendor paths, and
# libva here has no X11 backend, which Avidemux's VA-API code needs. AOM,
# FAAC, TwoLAME, Aften, dcaenc, libdca, Xvid, OpenCORE AMR and VapourSynth
# have no ports; FDK AAC is left out so the build stays redistributable, and
# the FFmpeg AAC encoder takes its place. SpiderMonkey is the one script
# engine that needs an outside library; tinyPy is built in. The Qt GUI links
# libX11 on every Unix build, whichever platform plugin it runs on.
_features="-DQT6=ON -DQT5=OFF -DQT4=OFF -DGTK=OFF -DOPENGL=ON -DSDL=OFF
	-DALSA=ON -DPULSEAUDIO=ON -DPULSEAUDIOSIMPLE=ON -DJACK=OFF -DARTS=OFF
	-DESD=OFF -DOSS=OFF -DVDPAU=OFF -DXVBA=OFF -DNVENC=OFF -DXVIDEO=OFF
	-DLIBVA=OFF -DVAPOURSYNTH=OFF -DX264=ON -DX265=ON -DVPXDEC=ON
	-DVPXENC=ON -DAOMDEC=OFF -DOPUS=ON -DOPUS_ENCODER=ON -DLAME=ON
	-DVORBIS=ON -DLIBVORBIS=ON -DFAAD=ON -DFDK_AAC=OFF -DFAAC=OFF
	-DTWOLAME=OFF -DAFTEN=OFF -DDCAENC=OFF -DLIBDCA=OFF -DXVID=OFF
	-DOPENCORE_AMRNB=OFF -DOPENCORE_AMRWB=OFF -DFREETYPE2=ON
	-DFONTCONFIG=ON -DFRIBIDI=ON -DGETTEXT=OFF -DSPIDERMONKEY=OFF
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

# English only: the GUI falls back to its source strings.
rm -rf "$PKG/usr/share/avidemux6/qt6/i18n"

# UPSTREAM'S ENTRY IS REPLACED: it runs avidemux3_qt5, which this build does
# not make, and has no StartupWMClass. With no organisation domain set, Qt
# takes the Wayland app_id from the program name, avidemux3_qt6. It claims no
# MimeType: every one it lists belongs to the players. The 128 px PNG is
# upstream's.
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
