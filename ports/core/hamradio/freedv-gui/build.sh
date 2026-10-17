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

_be="$SRC_ROOT/freedv-backend-$_backend"
_rade_src="$SRC_ROOT/rade_c-$_rade"
_opus_src="$SRC_ROOT/opus-$_opus"
_rnnoise_src="$SRC_ROOT/rnnoise-$_rnnoise"
patch -p1 -i "$PORT_SRC/musl.patch"
patch -d "$_be" -p1 -i "$PORT_SRC/backend-local-sources.patch"
patch -d "$_be" -p1 -i "$PORT_SRC/backend-musl.patch"
patch -d "$_rade_src" -p1 -i "$PORT_SRC/rade-opus-no-model-download.patch"
cp -a "$SRC_ROOT/dnn/." "$_opus_src/dnn/"
cp -a "$SRC_ROOT"/src/rnnoise_data* "$_rnnoise_src/src/"

export X86_AVX2_CFLAGS="-mavx -mfma -mavx2 -Wno-error=incompatible-pointer-types"
export LDFLAGS="$LDFLAGS -Wl,--strip-debug"
cmake -B build \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
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

install -d "$PKG/usr/lib"
cp -a build/_deps/freedv_backend-build/rade_build/src/librade.so* "$PKG/usr/lib/"

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
