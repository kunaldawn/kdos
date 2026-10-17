# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

mkdir -p build/javacc-src build/javacc
unzip -q "javacc-$_javacc-sources.jar" -d build/javacc-src
find build/javacc-src -name '*.java' | LC_ALL=C sort > build/javacc.list
javac -encoding UTF-8 -nowarn -proc:none -d build/javacc @build/javacc.list
cp -r build/javacc-src/templates build/javacc-src/version.properties build/javacc/
_mapcss=src/org/openstreetmap/josm/gui/mappaint/mapcss
( cd "$_mapcss" && java -cp "$SRC/build/javacc" javacc \
	-DEBUG_PARSER=false -DEBUG_TOKEN_MANAGER=false -JDK_VERSION=1.11 \
	-GRAMMAR_ENCODING=UTF-8 -UNICODE_INPUT=true MapCSSParser.jj )

_dep=build/depsrc
mkdir -p "$_dep" build/classes
for _j in jmapviewer-$_jmapviewer jakarta.json-api-$_jsonapi parsson-$_parsson \
	commons-jcs3-core-$_jcs commons-compress-$_compress commons-io-$_io \
	commons-lang3-$_lang3 commons-codec-$_codec \
	jakarta.annotation-api-$_annotation xz-$_xz \
	metadata-extractor-$_metadata annotations-$_jbannot \
	error_prone_annotations-$_errorprone biz.aQute.bnd.annotation-$_bnd \
	osgi.annotation-$_osgiannot org.osgi.namespace.extender-$_osgiext \
	org.osgi.service.serviceloader-$_osgisl org.osgi.resource-$_osgires \
	OpeningHoursParser-$_ohp; do
	unzip -q -o "$_j-sources.jar" -d "$_dep"
done
cp -r "$SRC_ROOT/adobe-xmp-core-$_xmp/com" "$_dep/"
cp -r "$SRC_ROOT/jsvg-$_jsvg/jsvg/src/main/java/." "$SRC_ROOT/jsvg-$_jsvg/annotations/src/main/java/." "$_dep/"
( cd "$_dep/ch/poole/openinghoursparser" && java -cp "$SRC/build/javacc" javacc \
	-GRAMMAR_ENCODING=UTF-8 OpeningHoursParser.jj )
rm -rf "$_dep/META-INF/maven" "$_dep/META-INF/MANIFEST.MF" "$_dep/META-INF/versions"
find "$_dep" -name module-info.java -delete

_jcs=$_dep/org/apache/commons/jcs3
rm -rf "$_jcs/auxiliary/disk/jdbc" "$_jcs/auxiliary/remote/http" "$_jcs/utils/servlet" \
	"$_jcs/auxiliary/remote/server/RemoteCacheStartupServlet.java" \
	"$_jcs"/log/Log4j2*.java
{
	find src -name '*.java' ! -name package-info.java ! -name module-info.java
	find "$_dep/org/eclipse/parsson" "$_jcs" -name '*.java' ! -name package-info.java
} | LC_ALL=C sort > build/sources.list
javac -encoding UTF-8 -nowarn -proc:none -implicit:class --release 21 \
	-d build/classes -sourcepath "$_dep:src" @build/sources.list

mkdir -p build/epsg resources/data/projection
: > resources/data/projection/custom-epsg
javac -encoding UTF-8 -nowarn -d build/epsg -cp build/classes scripts/BuildProjectionDefinitions.java
java -Djava.awt.headless=true -cp "resources:build/classes:build/epsg" BuildProjectionDefinitions .

_date=$(date -u -d "@$SOURCE_DATE_EPOCH" '+%Y-%m-%d %H:%M:%S')
printf 'Revision: %s\nBuild-Date: %s\n' "$version" "$_date" > resources/REVISION
cp CONTRIBUTION README LICENSE resources/

find resources/data -maxdepth 1 -name '*.lang' ! -name 'en*.lang' -delete

cp -r resources/. build/classes/
( cd "$_dep" && find . -type f ! -name '*.java' ! -name '*.jj' ! -name '*.html' \
	! -path './META-INF/LICENSE*' ! -path './META-INF/NOTICE*' \
	-exec cp --parents {} "$SRC/build/classes/" \; )
unzip -q -o "tag2link-$_tag2link.jar" 'META-INF/resources/webjars/tag2link/*/index.json' \
	-d build/classes

cat > build/manifest.txt <<MANIFEST
Main-Class: org.openstreetmap.josm.gui.MainApplication
Main-Version: $version SVN
Main-Date: $_date
Application-Name: JOSM - Java OpenStreetMap Editor
Add-Exports: java.base/sun.security.action java.desktop/com.sun.imageio.spi java.desktop/com.sun.imageio.plugins.jpeg
Add-Opens: java.base/java.lang java.base/java.nio java.base/jdk.internal.loader java.base/jdk.internal.ref java.desktop/javax.imageio.spi java.desktop/javax.swing.text.html java.prefs/java.util.prefs
MANIFEST
( cd build/classes && find . -type f | LC_ALL=C sort | sed 's/.*/"&"/' > ../classes.list )
( cd build/classes && jar --create --file ../josm.jar --manifest ../manifest.txt \
	--date="$(date -u -d "@$SOURCE_DATE_EPOCH" '+%Y-%m-%dT%H:%M:%SZ')" @../classes.list )

install -Dm644 build/josm.jar "$PKG/usr/share/josm/josm.jar"
install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/josm" <<'LAUNCHER'
#!/bin/sh
_first=$1
set -- java -XX:MaxRAMPercentage=75.0 -Djava.net.useSystemProxies=true \
	--add-modules java.scripting,java.sql \
	--add-exports=java.base/sun.security.action=ALL-UNNAMED \
	--add-exports=java.desktop/com.sun.imageio.plugins.jpeg=ALL-UNNAMED \
	--add-exports=java.desktop/com.sun.imageio.spi=ALL-UNNAMED \
	-jar /usr/share/josm/josm.jar "$@"
case $_first in
render|validate|project) exec "$@" ;;
esac
exec "$@" --offline=JOSM_WEBSITE
LAUNCHER
chmod 755 "$PKG/usr/bin/josm"

_n=native/linux/tested/usr/share
install -Dm644 "$_n/man/man1/josm.1" "$PKG/usr/share/man/man1/josm.1"
install -Dm644 "$_n/mime/packages/josm.xml" "$PKG/usr/share/mime/packages/josm.xml"
install -Dm644 "$_n/metainfo/org.openstreetmap.josm.appdata.xml" \
	"$PKG/usr/share/metainfo/org.openstreetmap.josm.appdata.xml"
for _i in "$_n"/icons/hicolor/*/apps/org.openstreetmap.josm.png; do
	_s=${_i#"$_n/icons/hicolor/"}
	install -Dm644 "$_i" "$PKG/usr/share/icons/hicolor/$_s"
done

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/org.openstreetmap.josm.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=JOSM
GenericName=OpenStreetMap Editor
Comment=Edit OpenStreetMap data, GPS traces and map imagery
Exec=josm %U
Icon=org.openstreetmap.josm
Terminal=false
StartupWMClass=org-openstreetmap-josm-gui-MainApplication
Categories=Education;Geography;Maps;
MimeType=application/vnd.openstreetmap.data+xml;application/x-josm-session+xml;application/x-josm-session+zip;
Keywords=openstreetmap;osm;map;editor;gps;gpx;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.openstreetmap.josm.desktop"
