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

patch -p1 -i "$PORT_SRC/nodevel.patch"
rmdir data/doc
mv "$SRC_ROOT/documentation-$version" data/doc

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
	-D WANT_JACK=ON \
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

rm -rf "$PKG/usr/share/hydrogen/data/i18n"
rm -f "$PKG"/usr/share/hydrogen/data/doc/tutorial_fr.html \
	"$PKG"/usr/share/hydrogen/data/doc/tutorial_it.html
test -f "$PKG/usr/share/hydrogen/data/doc/manual_en.html"

for _size in 32 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps"
	rsvg-convert -w "$_size" -h "$_size" \
		-o "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps/org.hydrogenmusic.Hydrogen.png" \
		"$PKG/usr/share/icons/hicolor/scalable/apps/org.hydrogenmusic.Hydrogen.svg"
done

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
