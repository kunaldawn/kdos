# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# Offline use is maps from MBTiles and the tile cache: the tile and search
# services that need an account or a network (Bing, Google, Terraserver,
# Expedia, Blue Marble, GeoNames, geocaches, USGS DEM download, OSM upload)
# are off. Mapnik has no port. libnova gives the sunrise and sunset times.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--disable-gtk2 \
	--disable-deprecations \
	--disable-bing \
	--disable-google \
	--disable-terraserver \
	--disable-expedia \
	--enable-openstreetmap \
	--disable-osm-auth \
	--disable-oauth \
	--disable-bluemarble \
	--disable-geonames \
	--disable-geocaches \
	--disable-dem24k \
	--enable-geoclue \
	--enable-geotag \
	--enable-realtime-gps-tracking \
	--enable-bzip2 \
	--enable-magic \
	--enable-mbtiles \
	--enable-zip \
	--enable-xz \
	--enable-nettle \
	--disable-mapnik \
	--enable-nova
make
make DESTDIR=$PKG install

# English only: the other interface catalogues go.
find "$PKG/usr/share/locale" -mindepth 1 -maxdepth 1 ! -name 'en*' -exec rm -rf {} +

install -Dm644 viking-128.png "$PKG/usr/share/icons/hicolor/128x128/apps/viking.png"
install -Dm644 org.viking.Viking.appdata.xml "$PKG/usr/share/metainfo/org.viking.Viking.appdata.xml"

# GTK 3 takes the Wayland app_id from the program name.
cat > "$PKG/usr/share/applications/viking.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Viking
GenericName=GPS Data Manager
Comment=Manage GPS tracks, waypoints and routes on maps
Exec=viking %F
Icon=viking
Terminal=false
StartupWMClass=viking
Categories=GTK;Science;Geography;Maps;Education;
Keywords=gps;gpx;kml;track;waypoint;route;map;mbtiles;
DESKTOP
chmod 644 "$PKG/usr/share/applications/viking.desktop"
