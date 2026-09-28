# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# THIS IS THE SDL 1.2 AN SDL 1.2 PROGRAM LINKS: libSDL-1.2.so.0, the SDL 1.2
# headers under /usr/include/SDL, sdl12_compat.pc (`Provides: sdl`) and sdl.m4.
# It carries no drivers: it dlopens libSDL2-2.0.so.0 at SDL_Init, which is
# sdl2-compat over SDL3, so the backends are the ones the sdl3 port chose.
#
# sdl-config stays. SDL 1.2 programs predate pkg-config for SDL and their
# makefiles run `sdl-config --cflags --libs` directly; this one is generated
# for /usr and answers what the .pc file answers.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DSDL12TESTS=OFF \
	-DSDL12DEVEL=ON \
	-DSTATICDEVEL=OFF
ninja
DESTDIR=$PKG ninja install

test -e "$PKG/usr/lib/libSDL-1.2.so.0"
