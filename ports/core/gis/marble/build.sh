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

# KF_SKIP_PO_PROCESSING leaves every translation out: bundled data is English
# only. KF6 Parts makes this the KDE application (marble), not the plain Qt
# one; Behaim and Marble Maps are built beside it and need Kirigami at run
# time. The optional finds are made explicit: OSM PBF files read through
# protobuf and abseil, positions come from gpsd and Qt Positioning, the APRS
# layer reads a serial port, and shapefiles open through shapelib. Phonon
# (spoken routing), the Plasma applet and KRunner plugin, libwlocate and the
# Qt Designer plugin are off. MOBILE stays off: it replaces the desktop data
# set with pre-cut tiles for handhelds.
cmake -S . -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_SYSCONFDIR=/etc \
	-DKDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-DBUILD_TESTING=OFF \
	-DKF_SKIP_PO_PROCESSING=ON \
	-DMOBILE=OFF \
	-DBUILD_TOUCH=OFF \
	-DBUILD_QT_AND_KDE=OFF \
	-DBUILD_MARBLE_APPS=ON \
	-DBUILD_MARBLE_TOOLS=OFF \
	-DBUILD_MARBLE_EXAMPLES=OFF \
	-DBUILD_WITH_DBUS=ON \
	-DWITH_DESIGNER_PLUGIN=OFF \
	-DWITH_libgps=ON \
	-DWITH_libshp=ON \
	-DWITH_ZLIB=ON \
	-DWITH_Phonon4Qt6=OFF \
	-DWITH_Plasma=OFF \
	-DWITH_libwlocate=OFF \
	-Wno-dev
for v in Protobuf_PROTOC_EXECUTABLE LIBGPS_LIBRARIES LIBSHP_LIBRARIES; do
	grep -Eq "^$v:[A-Z]+=/" build/CMakeCache.txt ||
		{ echo "marble: $v not found at configure" >&2; exit 1; }
done
cmake --build build
DESTDIR=$PKG cmake --install build
[ -f "$PKG/usr/share/icons/hicolor/48x48/apps/marble.png" ] ||
	{ echo "marble: the hicolor PNG icon was not installed" >&2; exit 1; }

# Bundled data is English only. KF_SKIP_PO_PROCESSING does not reach Marble's
# own Qt catalogues (poqm/, built whenever lconvert is found) or the handbooks
# kdoctools_install() builds from po/, so both are removed here.
rm -rf "$PKG/usr/share/locale"
if [ -d "$PKG/usr/share/doc/HTML" ]; then
	find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +
fi

# Behaim and Marble Maps install their icons as SVG only, which nothing in
# the session rasterises; each is rendered to hicolor PNGs under its name.
for icon in org.kde.marble.behaim org.kde.marble.maps; do
	svg="$PKG/usr/share/icons/hicolor/scalable/apps/$icon.svg"
	[ -f "$svg" ] || { echo "marble: $icon.svg was not installed" >&2; exit 1; }
	for s in 48 128 256; do
		install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
		rsvg-convert -w $s -h $s "$svg" \
			-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/$icon.png"
	done
done

# UPSTREAM'S ENTRIES ARE REPLACED for StartupWMClass. KAboutData derives each
# desktop file name, and so each Wayland app_id: org.kde.marble for the
# globe, and the names Behaim and Marble Maps set explicitly. The globe keeps
# the geo: and worldwind: links, which nothing else claims.
cat > "$PKG/usr/share/applications/org.kde.marble.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KDE Marble
GenericName=Virtual Globe
Comment=Virtual globe and world atlas
TryExec=marble
Exec=marble %U
Icon=marble
Terminal=false
StartupWMClass=org.kde.marble
X-DocPath=marble/index.html
MimeType=x-scheme-handler/geo;x-scheme-handler/worldwind;
Categories=Qt;KDE;Education;Geoscience;Geography;Science;Maps;
Keywords=globe;atlas;map;maps;earth;geography;osm;openstreetmap;marble;
DESKTOP
cat > "$PKG/usr/share/applications/org.kde.marble.behaim.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Marble Behaim
GenericName=Historic Globe
Comment=The Behaim globe of 1492, the oldest surviving terrestrial globe
TryExec=marble-behaim
Exec=marble-behaim
Icon=org.kde.marble.behaim
Terminal=false
StartupWMClass=org.kde.marble.behaim
Categories=Qt;KDE;Education;Geoscience;Geography;Science;Maps;
Keywords=globe;behaim;history;map;marble;
DESKTOP
cat > "$PKG/usr/share/applications/org.kde.marble.maps.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Marble Maps
GenericName=OpenStreetMap Navigation
Comment=Find your way on OpenStreetMap
TryExec=marble-maps
Exec=marble-maps %F
Icon=org.kde.marble.maps
Terminal=false
StartupWMClass=org.kde.marble.maps
Categories=Qt;KDE;Education;Geoscience;Geography;Science;Maps;
Keywords=map;maps;navigation;route;osm;openstreetmap;marble;
DESKTOP
chmod 644 "$PKG"/usr/share/applications/org.kde.marble*.desktop
