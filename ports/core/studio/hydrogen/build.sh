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

# A tarball is not a git checkout, so upstream's tag probe calls every build
# a development one and the main window opens with a warning saying so. The
# patch is Alpine's, and marks the build as a release.
patch -p1 -i "$PORT_SRC/nodevel.patch"

# Audio and MIDI through ALSA and PulseAudio (pipewire-pulse on this system).
# JACK is not on this system, and PortAudio and PortMidi would be second
# routes to the same devices. LADSPA effects with LRDF metadata, and OSC
# remote control through liblo. Rubber Band is off: upstream marks its use
# experimental, with wrong timing. The test suite and the API reference are
# not built.
export CMAKE_CXX_FLAGS="$CXXFLAGS"
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON \
	-D WANT_QT6=ON \
	-D WANT_SHARED=ON \
	-D WANT_DEBUG=OFF \
	-D WANT_LIBARCHIVE=ON \
	-D WANT_ALSA=ON \
	-D WANT_PULSEAUDIO=ON \
	-D WANT_LADSPA=ON \
	-D WANT_LRDF=ON \
	-D WANT_OSC=ON \
	-D WANT_JACK=OFF \
	-D WANT_PORTAUDIO=OFF \
	-D WANT_PORTMIDI=OFF \
	-D WANT_OSS=OFF \
	-D WANT_LASH=OFF \
	-D WANT_RUBBERBAND=OFF \
	-D WANT_APPIMAGE=OFF \
	-D WANT_CPPUNIT=OFF \
	-D WANT_INTEGRATION_TESTS=OFF \
	-D WANT_CLANG_TIDY=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# Bundled data is English only; the interface falls back to its source
# strings.
rm -rf "$PKG/usr/share/hydrogen/data/i18n"

# The application icon is installed as SVG only, which the panel never reads.
for _size in 32 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps"
	rsvg-convert -w "$_size" -h "$_size" \
		-o "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps/org.hydrogenmusic.Hydrogen.png" \
		"$PKG/usr/share/icons/hicolor/scalable/apps/org.hydrogenmusic.Hydrogen.svg"
done

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: no desktop file name is
# set, so the Wayland app_id is the executable's name, hydrogen. Upstream's
# MimeType claims text/xml, every XML file on the system, and is left out.
cat > "$PKG/usr/share/applications/org.hydrogenmusic.Hydrogen.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Hydrogen
GenericName=Drum Machine
Comment=Create drum patterns and songs
TryExec=hydrogen
Exec=hydrogen %F
Icon=org.hydrogenmusic.Hydrogen
Terminal=false
StartupNotify=true
StartupWMClass=hydrogen
Categories=AudioVideo;Audio;Midi;Sequencer;Qt;
Keywords=audio;sound;drum;drums;machine;sequencer;sampler;beat;hydrogen;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.hydrogenmusic.Hydrogen.desktop"
