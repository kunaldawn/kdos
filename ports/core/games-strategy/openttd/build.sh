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

# fix-segfault is Alpine's: the ICU line breaker is handed a temporary string
# that is gone before it is read. no-survey removes the opt-in usage survey,
# its settings button and its question at the first network game, so the
# program never offers to post anything.
patch -p1 -i "$PORT_SRC/fix-segfault.patch"
patch -p1 -i "$PORT_SRC/no-survey.patch"

# The graphics, sound and music base sets are the openttd-open* ports; with
# none installed the game refuses to start. musl gives a new thread 128 KiB of
# stack unless the executable asks for more, and the pathfinder and script
# threads overflow that, so the link asks for 1 MiB. CURL_NO_CURL_CMAKE sends
# FindCURL straight to pkg-config: curl installs no CMake package, and the
# requirement would apply to FindCURL's search for one as well.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_BINDIR=bin \
	-DCMAKE_INSTALL_DATADIR=share/games \
	-DOPTION_INSTALL_FHS=ON \
	-DOPTION_DEDICATED=OFF \
	-DOPTION_USE_ASSERTS=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_Allegro=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_unofficial-breakpad=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Grfcodec=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_SDL2=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Freetype=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Fontconfig=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Harfbuzz=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_ICU=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Fluidsynth=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_OpusFile=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_PNG=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_ZLIB=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LibLZMA=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LZO=ON \
	-DCURL_NO_CURL_CMAKE=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_CURL=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_OpenGL=ON \
	-DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS -Wl,-z,stack-size=1048576"
cmake --build build
DESTDIR=$PKG cmake --install build

# English only: every other language file is dropped. The in-game list then
# offers English alone, and a config naming another language falls back to it.
find "$PKG/usr/share/games/openttd/lang" -name '*.lng' ! -name 'english*.lng' -delete

# Upstream's entry carries no window class. The window's app_id is the
# program's name.
cat > "$PKG/usr/share/applications/openttd.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=OpenTTD
GenericName=Transport Simulation
Comment=Build rail, road, air and sea transport networks
Exec=openttd
Icon=openttd
Terminal=false
StartupWMClass=openttd
Categories=Game;Simulation;
Keywords=game;simulation;transport;tycoon;deluxe;economics;train;ship;bus;truck;aircraft;cargo;
DESKTOP
chmod 644 "$PKG/usr/share/applications/openttd.desktop"
