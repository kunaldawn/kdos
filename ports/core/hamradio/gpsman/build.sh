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

patch -p1 -i "$PORT_SRC/gpsman-system-paths.patch"

install -Dm755 gpsman.tcl "$PKG/usr/bin/gpsman"
install -d "$PKG/usr/share/gpsman"
cp -r gmsrc "$PKG/usr/share/gpsman/"
find "$PKG/usr/share/gpsman" -type d -exec chmod 755 {} +
find "$PKG/usr/share/gpsman" -type f -exec chmod 644 {} +
install -Dm644 man/man1/gpsman.1 "$PKG/usr/share/man/man1/gpsman.1"

_shp="$SRC_ROOT/gpsmanshp_$_shpver"
_shpdir="$PKG/usr/lib/gpsmanshp$_shpver"
patch -d "$_shp" -p1 -i "$PORT_SRC/gpsmanshp-tcl9.patch"
install -d "$_shpdir"
$CC $CFLAGS -c "$_shp/gpsmanshp.c" -o "$_shp/gpsmanshp.o"
$CC -shared $LDFLAGS -o "$_shpdir/gpsmanshp.so" "$_shp/gpsmanshp.o" -lshp -ltcl9.0
printf 'package ifneeded gpsmanshp %s [list load [file join $dir gpsmanshp.so] Gpsmanshp]\n' \
	"$_shpver" > "$_shpdir/pkgIndex.tcl"
chmod 644 "$_shpdir/pkgIndex.tcl"
printf 'set auto_path [linsert $auto_path 0 %s]\nputs [package require gpsmanshp]\n' \
	"$PKG/usr/lib" | tclsh9.0 | grep -qx "$_shpver"

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	magick manual/html/gpsman-logo.gif -background none -gravity center \
		-extent 375x375 -resize ${s}x${s} \
		"$PKG/usr/share/icons/hicolor/${s}x${s}/apps/gpsman.png"
done
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/gpsman.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=GPSMan
GenericName=GPS Manager
Comment=Transfer waypoints, routes and tracks to and from a GPS receiver
Exec=gpsman
Icon=gpsman
Terminal=false
StartupWMClass=Gpsman
Categories=Utility;Geography;HamRadio;
Keywords=gps;garmin;lowrance;magellan;waypoint;route;track;shapefile;
DESKTOP
chmod 644 "$PKG/usr/share/applications/gpsman.desktop"

test -x "$PKG/usr/bin/gpsman"
test -f "$PKG/usr/share/gpsman/gmsrc/main.tcl"
