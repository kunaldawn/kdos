# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/fix-cmake-policy.patch"

cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_CXX_FLAGS="$CXXFLAGS -Wno-deprecated-declarations $(pkg-config --cflags cairo)" \
	-DFORCE_VERSION=$version \
	-DBASHCOMP_PKG_PATH="$PKG/usr/share/bash-completion/completions" \
	-DWANT_QT5=ON \
	-DWANT_ALSA=ON \
	-DWANT_PULSEAUDIO=ON \
	-DWANT_PORTAUDIO=ON \
	-DWANT_JACK=ON \
	-DWANT_WEAKJACK=OFF \
	-DWANT_CARLA=OFF \
	-DWANT_VST=OFF \
	-DWANT_SDL=ON \
	-DWANT_SNDIO=OFF \
	-DWANT_SOUNDIO=ON \
	-DWANT_GIG=ON \
	-DWANT_STK=ON \
	-DWANT_SF2=ON \
	-DWANT_MP3LAME=ON \
	-DWANT_OGGVORBIS=ON \
	-DWANT_CALF=ON \
	-DWANT_CAPS=ON \
	-DWANT_CMT=ON \
	-DWANT_SWH=ON \
	-DWANT_TAP=ON

for _h in LMMS_HAVE_ALSA LMMS_HAVE_PULSEAUDIO LMMS_HAVE_PORTAUDIO LMMS_HAVE_JACK \
	LMMS_HAVE_SOUNDIO LMMS_HAVE_SDL LMMS_HAVE_STK; do
	grep -qx "#define $_h" build/lmmsconfig.h
done

cmake --build build
DESTDIR=$PKG cmake --install build

cat > "$PKG/usr/share/applications/lmms.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=LMMS
GenericName=Music Production
Comment=Compose music with a pattern sequencer, synthesizers, samples and effects
Exec=lmms %f
Icon=lmms
Terminal=false
StartupWMClass=lmms
Categories=Qt;AudioVideo;Audio;Midi;Sequencer;
MimeType=application/x-lmms-project;
Keywords=music;daw;sequencer;synthesizer;beat;midi;sample;
DESKTOP
chmod 644 "$PKG/usr/share/applications/lmms.desktop"

test -e "$PKG/usr/lib/lmms/libmalletsstk.so"
test -e "$PKG/usr/lib/lmms/libgigplayer.so"
test -f "$PKG/usr/share/bash-completion/completions/lmms"
