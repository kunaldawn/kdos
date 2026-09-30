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

# The effects, transitions and generators are MLT's and frei0r's, found at run
# time. Speech-to-text and object masking build a Python venv with pip and
# download their models when first asked for, so offline both report the
# missing module and the editor goes on without them; the online-resource
# providers and KNewStuff downloads fail the same way. CRASH_AUTO_TEST is the
# one option that would download RTTR into the build, and stays off.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D RELEASE_BUILD=ON \
	-D USE_DBUS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_QCH=OFF \
	-D BUILD_FUZZING=OFF \
	-D CRASH_AUTO_TEST=OFF \
	-D BUILD_DESIGNERPLUGIN=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# English only: the translated strings and handbooks are not shipped; the
# English handbook is.
rm -rf "$PKG/usr/share/locale"
find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: the Wayland app_id is the
# desktop file name Kdenlive sets, org.kde.kdenlive. It claims only its own
# project type: the media types belong to the players and viewers. The
# hicolor PNGs are upstream's.
cat > "$PKG/usr/share/applications/org.kde.kdenlive.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Kdenlive
GenericName=Video Editor
Comment=Edit video on a multi-track timeline
Exec=kdenlive %F
Icon=kdenlive
Terminal=false
StartupWMClass=org.kde.kdenlive
X-DocPath=kdenlive/index.html
MimeType=application/x-kdenlive;
Categories=Qt;KDE;AudioVideo;AudioVideoEditing;
Keywords=editing;video;audio;mlt;timeline;cut;kdenlive;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kdenlive.desktop"
