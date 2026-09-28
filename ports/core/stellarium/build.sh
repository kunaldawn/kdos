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

# QXlsx, the INDI client and md4c are fetched by CPM at configure time
# upstream. Each is a source of this port instead, pointed at through CPM's
# per-package override, so configure never reaches for the network. NLopt,
# which the lens distortion estimator fits with, is the nlopt port: CPM looks
# for an installed copy before it downloads one, so it is required by name
# rather than taken from whatever the build root holds. kpkg copies a .zip
# into $SRC whole, so QXlsx and INDI are unpacked here.
cmake -E chdir "$SRC_ROOT" cmake -E tar xf "$SRC/stellarium-qxlsx-$_qxlsx.zip"
cmake -E chdir "$SRC_ROOT" cmake -E tar xf "$SRC/stellarium-indi-$_indi.zip"

# ENABLE_PODIR=0 leaves the interface catalogues out: bundled data is English
# only. The ShowMySky atmosphere needs CalcMySky and a precomputed model, and
# the web views need QtWebEngine for pages that are only online; both are off,
# with the Online Queries plugin, which looks objects up on web services. GPS
# through gpsd, exiv2 for the lens distortion estimator and speech output are
# each dropped silently when missing, so the first two are required and
# qt6-qtspeech is in depends.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D STELLARIUM_RELEASE_BUILD=1 \
	-D ENABLE_TESTING=0 \
	-D ENABLE_CCACHE=0 \
	-D ENABLE_PODIR=0 \
	-D ENABLE_SHOWMYSKY=0 \
	-D ENABLE_QTWEBENGINE=0 \
	-D ENABLE_GPS=1 \
	-D ENABLE_MEDIA=1 \
	-D ENABLE_SPEECH=1 \
	-D ENABLE_XLSX=1 \
	-D ENABLE_INDI=1 \
	-D USE_PLUGIN_ONLINEQUERIES=0 \
	-D PREFER_SYSTEM_INDILIB=0 \
	-D CMAKE_REQUIRE_FIND_PACKAGE_GPS=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_exiv2=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_NLopt=ON \
	-D CPM_QXlsxQt6_SOURCE="$SRC_ROOT/QXlsx-$_qxlsx" \
	-D CPM_indiclient_SOURCE="$SRC_ROOT/indi-$_indi" \
	-D CPM_md4c_SOURCE="$SRC_ROOT/md4c-release-$_md4c" \
	-D FETCHCONTENT_FULLY_DISCONNECTED=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# Four of the five faces upstream bundles are the same files noto-fonts and
# ttf-dejavu install, so the data directory links to those. Noto Sans SC has
# no single-face file on the system (noto-cjk ships a collection) and stays.
data="$PKG/usr/share/stellarium/data"
ln -sf /usr/share/fonts/noto/NotoSans-Regular.ttf "$data/NotoSans-Regular.ttf"
ln -sf /usr/share/fonts/noto/NotoSansMono-Regular.ttf "$data/NotoSansMono-Regular.ttf"
ln -sf /usr/share/fonts/TTF/DejaVuSans.ttf "$data/DejaVuSans.ttf"
ln -sf /usr/share/fonts/TTF/DejaVuSansMono.ttf "$data/DejaVuSansMono.ttf"

# The entry is replaced for StartupWMClass. Stellarium sets no desktop file
# name, so Qt makes the Wayland app_id from the reversed organisation domain
# and the program name: org.stellarium.stellarium. The script MIME type is
# defined by the stellarium.xml upstream installs.
rm -f "$PKG/usr/share/applications/org.stellarium.Stellarium.desktop"
cat > "$PKG/usr/share/applications/org.stellarium.stellarium.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Stellarium
GenericName=Planetarium
Comment=A realistic sky in 3D, as seen with the eye, binoculars or a telescope
TryExec=stellarium
Exec=stellarium --startup-script=%f
Icon=stellarium
Terminal=false
StartupWMClass=org.stellarium.stellarium
MimeType=application/x-stellarium-script;
Categories=Qt;Education;Science;Astronomy;
Keywords=astronomy;planetarium;sky;stars;planets;constellation;telescope;stellarium;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.stellarium.stellarium.desktop"
