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

# The tag archive carries its submodules empty. adpcm is compiled in and has
# no port, so it is a second source placed where the submodule sits; SDL and
# fmt, the other two, are ports and taken from the system instead.
cp -a "$SRC_ROOT/adpcm-$_adpcm/." extern/adpcm/

# Every library with a port is the system's: the vendored copies are left
# unbuilt. JACK stays out; audio is PortAudio over ALSA and SDL, and MIDI is
# RtMidi over ALSA. WITH_LOCALE=OFF because bundled data is English only.
# backward-cpp needs execinfo.h, which musl does not have. The OpenGL and
# SDL_Renderer backends are both built, so a machine with no usable GL still
# draws.
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_GUI=ON \
	-DSYSTEM_SDL2=ON \
	-DSYSTEM_FMT=ON \
	-DSYSTEM_FFTW=ON \
	-DSYSTEM_FREETYPE=ON \
	-DSYSTEM_LIBSNDFILE=ON \
	-DSYSTEM_PORTAUDIO=ON \
	-DSYSTEM_RTMIDI=ON \
	-DSYSTEM_ZLIB=ON \
	-DUSE_SDL2=ON \
	-DUSE_SNDFILE=ON \
	-DUSE_RTMIDI=ON \
	-DUSE_FREETYPE=ON \
	-DWITH_PORTAUDIO=ON \
	-DWITH_JACK=OFF \
	-DWITH_LOCALE=OFF \
	-DUSE_MOMO=OFF \
	-DUSE_BACKWARD=OFF \
	-DWITH_RENDER_SDL=ON \
	-DWITH_RENDER_OPENGL=ON \
	-DWITH_RENDER_OPENGL1=ON \
	-DUSE_GLES=OFF \
	-DWITH_DEMOS=OFF \
	-DWITH_INSTRUMENTS=ON \
	-DWITH_WAVETABLES=ON \
	-DWARNINGS_ARE_ERRORS=OFF
ninja
DESTDIR=$PKG ninja install
cd ..

# The translations are installed whatever WITH_LOCALE says.
rm -rf "$PKG/usr/share/locale"

# UPSTREAM'S ENTRY IS REPLACED: it takes no file argument. gui.cpp sets the
# SDL window class to org.tildearrow.furnace. audio/x-fur is the type this
# port's own mime.xml defines.
cat > "$PKG/usr/share/applications/furnace.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Furnace
GenericName=Chiptune Tracker
Comment=Compose music for the sound chips of old consoles and computers
TryExec=furnace
Exec=furnace %f
Icon=furnace
Terminal=false
StartupWMClass=org.tildearrow.furnace
Categories=AudioVideo;Audio;Sequencer;Music;
Keywords=chiptune;tracker;fm;psg;genesis;nes;gameboy;c64;music;
MimeType=audio/x-fur;
DESKTOP
chmod 644 "$PKG/usr/share/applications/furnace.desktop"
