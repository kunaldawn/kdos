# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

_dep=build/depsrc
mkdir -p "$_dep" build/classes
for _j in xstream-$_xstream mxparser-$_mxparser json-$_json slf4j-api-$_slf4j; do
	unzip -q -o "$_j-sources.jar" -d "$_dep"
done
rm -rf "$_dep/org/slf4j/impl"
unzip -q -o "slf4j-simple-$_slf4j-sources.jar" -d "$_dep"
rm -rf "$_dep/META-INF"

_opt='^import (net\.sf\.cglib|org\.dom4j|org\.jdom2?|nu\.xom|org\.kxml2|org\.xmlpull\.mxp1|org\.codehaus|org\.joda|com\.ctc|com\.bea|javax\.activation|javax\.xml\.bind)\.'
{
	find src/main/java -name '*.java'
	find "$_dep/com/thoughtworks/xstream" -name '*.java' -exec grep -LE "$_opt" {} +
} | LC_ALL=C sort > build/sources.list
javac -encoding UTF-8 -nowarn -proc:none -implicit:class \
	-d build/classes \
	-sourcepath "$_dep:$SRC_ROOT/xmlpull-api-v1-$_xmlpull/src/java/api:src/main/java" \
	@build/sources.list
cp -r src/main/resources/. build/classes/

_time=$(date -u -d "@$SOURCE_DATE_EPOCH" '+%Y-%m-%d %H:%M')
printf 'Main-Class: de.neemann.digital.gui.Main\nBuild-SCM-Revision: v%s-kdos\nBuild-Time: %s\nSplashScreen-Image: icons/splash.png\n' \
	"$version" "$_time" > build/manifest.txt
( cd build/classes && find . -type f | LC_ALL=C sort | sed 's/.*/"&"/' > ../classes.list )
( cd build/classes && jar --create --file ../Digital.jar --manifest ../manifest.txt \
	--date="$(date -u -d "@$SOURCE_DATE_EPOCH" '+%Y-%m-%dT%H:%M:%SZ')" @../classes.list )

install -Dm644 build/Digital.jar "$PKG/usr/share/digital/Digital.jar"
for _d in src/main/dig/*/; do
	install -d "$PKG/usr/share/digital/examples/$(basename "$_d")"
	cp -r "$_d". "$PKG/usr/share/digital/examples/$(basename "$_d")/"
done

install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/digital" <<'LAUNCHER'
#!/bin/sh
exec java -jar /usr/share/digital/Digital.jar "$@"
LAUNCHER
chmod 755 "$PKG/usr/bin/digital"

install -Dm644 distribution/linux/digital-simulator.xml \
	"$PKG/usr/share/mime/packages/digital-simulator.xml"
install -Dm644 src/main/resources/icons/icon128.png \
	"$PKG/usr/share/icons/hicolor/128x128/apps/digital.png"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/digital.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Digital
GenericName=Logic Simulator
Comment=Design and simulate digital logic circuits
Exec=digital %f
Icon=digital
Terminal=false
StartupWMClass=de-neemann-digital-gui-Main
Categories=Education;Electronics;Engineering;
MimeType=text/x-digital;
Keywords=logic;circuit;simulator;gate;flip-flop;fsm;vhdl;verilog;
DESKTOP
chmod 644 "$PKG/usr/share/applications/digital.desktop"
