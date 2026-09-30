# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# Upstream builds with Maven and bundles its dependencies' class files. Here
# every dependency is its published source: javac takes them on the source
# path and compiles exactly the classes Digital reaches, so optional
# integrations inside XStream that would need libraries nobody ships are
# never compiled.
_dep=build/depsrc
mkdir -p "$_dep" build/classes
for _j in xstream-$_xstream mxparser-$_mxparser json-$_json slf4j-api-$_slf4j; do
	unzip -q -o "$_j-sources.jar" -d "$_dep"
done
# slf4j-api's sources carry placeholder binders that its own jar leaves out;
# slf4j-simple's binders replace them.
rm -rf "$_dep/org/slf4j/impl"
unzip -q -o "slf4j-simple-$_slf4j-sources.jar" -d "$_dep"
rm -rf "$_dep/META-INF"

# XStream loads its mappers and converters by name at run time, so all of it
# is compiled except the drivers and converters for libraries this system
# does not carry (CGLIB, dom4j, JDOM, XOM, kXML2, XPP3, Jettison, Woodstox,
# Joda, JAXB, JavaBeans Activation), which XStream probes for and skips.
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

# The About box reads the revision and build time from the manifest. Digital
# asks api.github.com for a newer release only when the revision is a bare
# release tag of at most seven characters; this one is longer, so it never
# does.
_time=$(date -u -d "@$SOURCE_DATE_EPOCH" '+%Y-%m-%d %H:%M')
printf 'Main-Class: de.neemann.digital.gui.Main\nBuild-SCM-Revision: v%s-kdos\nBuild-Time: %s\nSplashScreen-Image: icons/splash.png\n' \
	"$version" "$_time" > build/manifest.txt
# One quoted name per line: some resource names contain spaces.
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

# Swing is an X11 client under Xwayland; its WM_CLASS is the main class
# with the dots turned into dashes.
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
