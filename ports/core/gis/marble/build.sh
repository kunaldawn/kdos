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

cmake -S . -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DKDE_INSTALL_LIBDIR=lib \
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

for icon in org.kde.marble.behaim org.kde.marble.maps; do
	svg="$PKG/usr/share/icons/hicolor/scalable/apps/$icon.svg"
	[ -f "$svg" ] || { echo "marble: $icon.svg was not installed" >&2; exit 1; }
	for s in 48 128 256; do
		install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
		rsvg-convert -w $s -h $s "$svg" \
			-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/$icon.png"
	done
done

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
