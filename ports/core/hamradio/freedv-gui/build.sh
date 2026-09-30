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

# EVERY SOURCE UPSTREAM FETCHES AT CONFIGURE TIME IS A LATER SOURCE HERE.
# freedv-backend comes through FetchContent, pointed at its unpacked tree.
# Inside it, RADE and RNNoise are ExternalProjects that clone a branch head,
# and RADE's Opus is a URL; backend-local-sources.patch turns all three into
# URL variables, set below to the pinned trees. Opus and RNNoise each download
# their network weights from media.xiph.org in autogen.sh: the weight archives
# are later sources, unpacked into $SRC_ROOT and copied into the two trees,
# and both configure with autoreconf instead of autogen.sh (the second patch,
# and the first for RNNoise), so nothing is fetched.
_be="$SRC_ROOT/freedv-backend-$_backend"
_rade_src="$SRC_ROOT/rade_c-$_rade"
_opus_src="$SRC_ROOT/opus-$_opus"
_rnnoise_src="$SRC_ROOT/rnnoise-$_rnnoise"
patch -d "$_be" -p1 -i "$PORT_SRC/backend-local-sources.patch"
patch -d "$_rade_src" -p1 -i "$PORT_SRC/rade-opus-no-model-download.patch"
cp -a "$SRC_ROOT/dnn/." "$_opus_src/dnn/"
cp -a "$SRC_ROOT"/src/rnnoise_data* "$_rnnoise_src/src/"

# The system libsamplerate, libebur128 and OpenSSL are used, so none of the
# backend's static fallbacks is built. USE_NATIVE_AUDIO is the PulseAudio
# engine, which pipewire-pulse answers. codec2 is built from the copy the
# release carries, which FreeDV links statically. The generator is Unix
# Makefiles, never Ninja: the Opus and RNNoise ExternalProjects build with a
# literal $(MAKE), which Ninja refuses to parse.
cmake -B build \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DFETCHCONTENT_FULLY_DISCONNECTED=ON \
	-DFETCHCONTENT_SOURCE_DIR_FREEDV_BACKEND="$_be" \
	-DRADE_C_URL="$_rade_src" \
	-DOPUS_URL="$_opus_src" \
	-DRNNOISE_URL="$_rnnoise_src" \
	-DUSE_NATIVE_AUDIO=TRUE \
	-DUSE_STATIC_DEPS=FALSE \
	-DBOOTSTRAP_WXWIDGETS=FALSE \
	-DDISABLE_TLS_SUPPORT=FALSE \
	-DUSE_STATIC_LIBRESSL=FALSE \
	-DUNITTEST=OFF \
	-DBUILD_BACKEND_UNITTESTS=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# RADE is a shared library the backend builds and upstream installs only in
# its macOS bundle; freedv links it by soname.
cp -a build/_deps/freedv_backend-build/rade_build/src/librade.so* "$PKG/usr/lib/"

# THE MENU: upstream installs the hicolor PNGs; its entry is replaced to carry
# the window class. wxGTK names the GTK program after the command, so the
# Wayland app_id is freedv.
cat > "$PKG/usr/share/applications/freedv.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=FreeDV
GenericName=HF Digital Voice
Comment=Digital voice for HF amateur radio over a sound card: RADE, 700D, 700E and 1600
Exec=freedv
Icon=freedv
Terminal=false
StartupWMClass=freedv
Categories=Network;HamRadio;AudioVideo;Audio;
Keywords=ham;radio;digital;voice;hf;codec2;rade;freedv;
DESKTOP
chmod 644 "$PKG/usr/share/applications/freedv.desktop"

test -x "$PKG/usr/bin/freedv"
test -e "$PKG/usr/lib/librade.so.0.1"
test -e "$PKG/usr/share/icons/hicolor/48x48/apps/freedv.png"
