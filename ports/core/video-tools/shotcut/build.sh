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

# SHOTCUT_NOUPGRADE compiles out the upgrade check, which asks shotcut.org for
# a newer version at start. libX11 is the window picker, reached only under
# Xwayland. Speech-to-text runs whisper-cli from beside the binary,
# /usr/bin, which the whisper.cpp port installs; the model is chosen in the
# dialog, and its download button is the one part that needs a network.
# The GitHub archive is the application alone: the release's shotcut-src
# bundle carries FFmpeg, MLT and the rest of its dependency tree.
# SHOTCUT_VERSION left unset is the build date, and two builds then differ.
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

# English only: Qt falls back to the source strings.
find "$PKG/usr/share/shotcut/translations" -name '*.qm' ! -name 'shotcut_en*' -delete

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: the Wayland app_id is the
# desktop file name Shotcut sets, org.shotcut.Shotcut, not "Shotcut". It
# claims only the MLT XML type it defines: the media types belong to the
# players. The hicolor PNGs are upstream's.
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
