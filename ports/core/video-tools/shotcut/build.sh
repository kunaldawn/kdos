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

export CXXFLAGS="$CXXFLAGS -DSHOTCUT_NOUPGRADE"
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D SHOTCUT_VERSION=$version \
	-D CLANG_FORMAT=OFF \
	-D BUILD_DOCS=OFF \
	-D EXTERNAL_LAUNCHERS=ON \
	-D USE_VULKAN=OFF \
	-D BUILD_MINIMAL_MEDIA_BACKEND=OFF \
	-D SHOTCUT_BUILD_TESTS=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

cat > "$PKG/usr/share/applications/org.shotcut.Shotcut.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Shotcut
GenericName=Video Editor
Comment=Edit video with filters and a timeline
Exec=shotcut %F
Icon=org.shotcut.Shotcut
Terminal=false
StartupWMClass=org.shotcut.Shotcut
MimeType=application/vnd.mlt+xml;
Categories=AudioVideo;Video;AudioVideoEditing;
Keywords=video;audio;editing;timeline;mlt;shotcut;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.shotcut.Shotcut.desktop"
