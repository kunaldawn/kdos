# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# A backend whose library is not found is dropped with a status line, not an
# error, so the two this port promises are checked in the generated header:
# PulseAudio (which PipeWire answers) and ALSA. JACK is not on this system.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_STATIC_LIBS=OFF \
	-DBUILD_DYNAMIC_LIBS=ON \
	-DBUILD_EXAMPLE_PROGRAMS=OFF \
	-DBUILD_TESTS=OFF \
	-DENABLE_JACK=OFF \
	-DENABLE_PULSEAUDIO=ON \
	-DENABLE_ALSA=ON \
	-DENABLE_COREAUDIO=OFF \
	-DENABLE_WASAPI=OFF
grep -q '^#define SOUNDIO_HAVE_PULSEAUDIO' config.h
grep -q '^#define SOUNDIO_HAVE_ALSA' config.h
ninja
DESTDIR=$PKG ninja install
