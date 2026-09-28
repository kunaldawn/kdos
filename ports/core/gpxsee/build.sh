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

# A qmake project. Qt 6's qmake reads no compiler flags from the environment,
# so the tree's flags, and with them the reproducibility maps, are passed in.
# lrelease is not run: bundled data is English only, and the project installs
# whatever .qm files exist, which is none. The map definitions under
# /usr/share/gpxsee/maps name online tile servers and draw nothing offline;
# local maps (mbtiles, mapsforge, KAP, IMG and the rest) open from the disk.
mkdir -p build && cd build
qmake6 .. \
	PREFIX=/usr \
	CONFIG+=release \
	QMAKE_CFLAGS+="$CFLAGS" \
	QMAKE_CXXFLAGS+="$CXXFLAGS" \
	QMAKE_LFLAGS+="$LDFLAGS"
make
make INSTALL_ROOT="$PKG" install
[ -f "$PKG/usr/share/icons/hicolor/48x48/apps/gpxsee.png" ] ||
	{ echo "gpxsee: the hicolor PNG icon was not installed" >&2; exit 1; }

# setDesktopFileName makes the Wayland app_id gpxsee. Upstream's entry also
# claims JPEG, TIFF, CSV, tar and MP4, which belong to the image viewer, the
# archive handler and the video player, and geo: links, which Marble claims;
# this one claims only GPS and map formats, which the XML installed above
# defines.
cat > "$PKG/usr/share/applications/gpxsee.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=GPXSee
GenericName=GPS Track Viewer
Comment=View and analyse GPS logs, routes and waypoints over offline maps
Exec=gpxsee %U
Icon=gpxsee
Terminal=false
StartupWMClass=gpxsee
MimeType=application/gpx+xml;application/vnd.garmin.tcx+xml;application/vnd.ant.fit;application/vnd.google-earth.kml+xml;application/vnd.fai.igc;application/vnd.nmea.nmea;application/vnd.oziexplorer.plt;application/vnd.oziexplorer.rte;application/vnd.oziexplorer.wpt;application/vnd.groundspeak.loc+xml;application/vnd.sigma.slf+xml;application/geo+json;application/vnd.naviter.seeyou.cup;application/vnd.garmin.gpi;application/vnd.suunto.sml+xml;application/vnd.garmin.img;application/vnd.garmin.jnx;application/vnd.garmin.gmap+xml;image/vnd.maptech.kap;application/vnd.oziexplorer.map;application/vnd.mapbox.mbtiles;application/vnd.twonav.rmap;application/vnd.trekbuddy.tba;application/vnd.gpxsee.map+xml;application/vnd.google-earth.kmz;application/vnd.alpinequest.aqm;application/vnd.cgtk.gemf;application/vnd.rmaps.sqlite;application/vnd.osmdroid.sqlite;application/vnd.mapsforge.map;application/vnd.tomtom.ov2;application/vnd.tomtom.itn;application/vnd.esri.wld;application/vnd.onmove.omd;application/vnd.onmove.ghp;application/vnd.memory-map.qct;application/vnd.twonav.trk;application/vnd.twonav.rte;application/vnd.twonav.wpt;application/vnd.orux.map+xml;application/vnd.iho.s57-data;application/vnd.iho.s57-catalogue;application/vnd.gpsdump.wpt;application/vnd.gpstuner.gmi;application/vnd.70mai.txt;application/vnd.velocitek.vtk;application/vnd.vakaros.vkx;application/vnd.coros.csa;application/vnd.coros.pma;application/vnd.pmtiles;
Categories=Qt;Education;Geography;Maps;Sports;Viewer;
Keywords=gps;gpx;track;route;waypoint;map;fit;kml;hiking;cycling;
DESKTOP
chmod 644 "$PKG/usr/share/applications/gpxsee.desktop"
