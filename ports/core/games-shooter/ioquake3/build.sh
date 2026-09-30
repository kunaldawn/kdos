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

# Every bundled library is replaced by the system's. PRODUCT_VERSION would
# otherwise come from `git describe`, and this archive has no repository.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr/lib/ioquake3 \
	-DPRODUCT_VERSION=$version \
	-DBUILD_CLIENT=ON \
	-DBUILD_SERVER=ON \
	-DBUILD_RENDERER_GL1=ON \
	-DBUILD_RENDERER_GL2=ON \
	-DBUILD_GAME_LIBRARIES=ON \
	-DBUILD_GAME_QVMS=ON \
	-DBUILD_STANDALONE=OFF \
	-DUSE_RENDERER_DLOPEN=ON \
	-DUSE_OPENAL=ON \
	-DUSE_OPENAL_DLOPEN=ON \
	-DUSE_HTTP=ON \
	-DUSE_CODEC_VORBIS=ON \
	-DUSE_CODEC_OPUS=ON \
	-DUSE_VOIP=ON \
	-DUSE_MUMBLE=ON \
	-DUSE_FREETYPE=ON \
	-DUSE_INTERNAL_LIBS=OFF \
	-DUSE_INTERNAL_SDL=OFF \
	-DUSE_INTERNAL_ZLIB=OFF \
	-DUSE_INTERNAL_JPEG=OFF \
	-DUSE_INTERNAL_OGG=OFF \
	-DUSE_INTERNAL_VORBIS=OFF \
	-DUSE_INTERNAL_OPUS=OFF \
	-DCMAKE_REQUIRE_FIND_PACKAGE_CURL=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_OpenAL=ON
cmake --build build
DESTDIR=$PKG cmake --install build

# The engine takes its own directory from argv[0] and loads the renderers and
# game modules from there, so it is started by its full path and /usr/bin holds
# scripts, not links. The game data is not free and is not shipped: a player
# puts pak0.pk3 and the point releases under ~/.q3a/baseq3.
install -d "$PKG/usr/bin"
for b in ioquake3 ioq3ded; do
	cat > "$PKG/usr/bin/$b" <<SCRIPT
#!/bin/sh
exec /usr/lib/ioquake3/$b "\$@"
SCRIPT
	chmod 755 "$PKG/usr/bin/$b"
done
install -Dm644 README.md ChangeLog COPYING.txt -t "$PKG/usr/share/doc/ioquake3"
install -Dm644 misc/linux/org.ioquake3.ioquake3.metainfo.xml \
	-t "$PKG/usr/share/metainfo"

# The panel reads no SVG: the menu icon is a PNG rasterised from upstream's.
# The window's app_id is the executable's name.
install -d "$PKG/usr/share/icons/hicolor/256x256/apps"
rsvg-convert -w 256 -h 256 misc/quake3.svg \
	-o "$PKG/usr/share/icons/hicolor/256x256/apps/ioquake3.png"
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/org.ioquake3.ioquake3.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=ioquake3
GenericName=First Person Shooter
Comment=Play Quake III Arena; needs the game data in ~/.q3a/baseq3
Exec=ioquake3
Icon=ioquake3
Terminal=false
StartupWMClass=ioquake3
Categories=Game;ActionGame;
Keywords=first;person;shooter;quake;quake3;arena;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.ioquake3.ioquake3.desktop"
