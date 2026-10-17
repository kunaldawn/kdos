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
mkdir -p "$_dep" build/classes build/proc build/gen build/docgen
for _j in rsyntaxtextarea-$_rsta colorpicker-$_colorpick \
	swingx-core-$_swingx swingx-common-$_swingx swingx-plaf-$_swingx \
	swingx-painters-$_swingx swingx-action-$_swingx swingx-graphics-$_swingx \
	swingx-autocomplete-$_swingx swingx-mavensupport-$_swingx swing-checkbox-tree-$_cbtree \
	slf4j-api-$_slf4j slf4j-simple-$_slf4j flatlaf-$_flatlaf commons-cli-$_cli \
	flexmark-$_flexmark flexmark-util-ast-$_flexmark flexmark-util-builder-$_flexmark \
	flexmark-util-collection-$_flexmark flexmark-util-data-$_flexmark \
	flexmark-util-dependency-$_flexmark flexmark-util-format-$_flexmark \
	flexmark-util-html-$_flexmark flexmark-util-misc-$_flexmark \
	flexmark-util-sequence-$_flexmark flexmark-util-visitor-$_flexmark \
	annotations-$_jbannot commons-text-$_text commons-lang3-$_lang3; do
	unzip -q -o "$_j-sources.jar" -d "$_dep"
done
patch -p1 -d "$SRC_ROOT/javahelp-$_javahelp" -i "$PORT_SRC/javahelp-no-awt-peer.patch"
_jh="$SRC_ROOT/javahelp-$_javahelp/jhMaster"
cp -r "$_jh/JavaHelp/src/new/." "$_jh/JavaHelp/src/impl/." "$_jh/JSearch/client/." "$_dep/"
rm -rf "$_dep/META-INF/maven" "$_dep/META-INF/MANIFEST.MF" "$_dep/META-INF/versions"
find "$_dep" -name module-info.java -delete
rm -rf "$_dep/javax/help/tagext" "$_dep/javax/help/ServletHelpBroker.java" \
	"$_dep/javax/help/plaf/basic/BasicNativeContentViewerUI.java"

patch -p1 -d "$_dep" -i "$PORT_SRC/colorpicker-no-lombok.patch"

unzip -q "metainf-services-$_mis-sources.jar" -d build/proc-src
javac -encoding UTF-8 -nowarn -d build/proc \
	build/proc-src/org/kohsuke/MetaInfServices.java \
	build/proc-src/org/kohsuke/metainf_services/AnnotationProcessorImpl.java
install -Dm644 build/proc-src/org/kohsuke/MetaInfServices.java "$_dep/org/kohsuke/MetaInfServices.java"

_gen=build/gen/com/cburch/logisim/generated
mkdir -p "$_gen"
_millis=$((SOURCE_DATE_EPOCH * 1000))
_year=$(date -u -d "@$SOURCE_DATE_EPOCH" +%Y)
_iso=$(date -u -d "@$SOURCE_DATE_EPOCH" '+%Y-%m-%dT%H:%M:%S+0000')
cat > "$_gen/BuildInfo.java" <<JAVA
package com.cburch.logisim.generated;

import com.cburch.logisim.LogisimVersion;
import java.util.Date;

public final class BuildInfo {
  public static final String branchName = "";
  public static final String branchLastCommitHash = "";
  public static final String buildId = "v$version";
  public static final long millis = ${_millis}L;
  public static final String year = "$_year";
  public static final String dateIso8601 = "$_iso";
  public static final Date date = new Date();
  static { date.setTime(millis); }
  public static final LogisimVersion version = LogisimVersion.fromString("$version");
  public static final String name = "Logisim-evolution";
  public static final String displayName = "Logisim-evolution v$version";
  public static final String url = "https://github.com/logisim-evolution/";
  public static final String jvm_version =
      String.format("%s v%s", System.getProperty("java.vm.name"), System.getProperty("java.version"));
  public static final String jvm_vendor = System.getProperty("java.vendor");
}
JAVA

{
	find src/main/java build/gen -name '*.java'
	find "$_dep/com/formdev/flatlaf" "$_dep/org/fife" "$_dep/javax/help" \
		"$_dep/com/sun/java/help" "$_dep/org/jdesktop/swingx/plaf" \
		"$_dep/org/slf4j/simple" -name '*.java' ! -name package-info.java
} | LC_ALL=C sort > build/sources.list
javac -encoding UTF-8 -nowarn -implicit:class --release 21 \
	-processorpath build/proc -processor org.kohsuke.metainf_services.AnnotationProcessorImpl \
	-d build/classes -sourcepath "$_dep:src/main/java:build/gen" @build/sources.list

cp -r src/main/resources/. build/classes/
( cd "$_dep" && find . -type f ! -name '*.java' ! -name '*.html' ! -name 'GNUmakefile' \
	! -path './META-INF/LICENSE*' ! -path './META-INF/NOTICE*' \
	-exec cp --parents {} "$SRC/build/classes/" \; )

javac -encoding UTF-8 -nowarn -d build/docgen $(find src/docgen/java -name '*.java')
java -cp build/docgen com.cburch.logisim.docs.DocumentationGenerator --help-sets \
	src/main/doc/help-sets.xml build/classes/doc build/classes/doc

( cd build/classes/doc && for _l in */; do
	_l=${_l%/}
	case $_l in en|icons|img-*) ;; *) [ -f "map_$_l.jhm" ] && rm -rf "$_l" "map_$_l.jhm" "search_lookup_$_l" "doc_$_l.hs" ;; esac
done )

printf 'Main-Class: com.cburch.logisim.Main\nEnable-Native-Access: ALL-UNNAMED\n' > build/manifest.txt
( cd build/classes && find . -type f | LC_ALL=C sort | sed 's/.*/"&"/' > ../classes.list )
( cd build/classes && jar --create --file ../logisim-evolution.jar --manifest ../manifest.txt \
	--date="$(date -u -d "@$SOURCE_DATE_EPOCH" '+%Y-%m-%dT%H:%M:%SZ')" @../classes.list )

install -Dm644 build/logisim-evolution.jar "$PKG/usr/share/logisim-evolution/logisim-evolution.jar"
install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/logisim-evolution" <<'LAUNCHER'
#!/bin/sh
exec java --enable-native-access=ALL-UNNAMED -jar /usr/share/logisim-evolution/logisim-evolution.jar "$@"
LAUNCHER
chmod 755 "$PKG/usr/bin/logisim-evolution"

install -Dm644 src/main/resources/resources/logisim/img/logisim-icon-128.png \
	"$PKG/usr/share/icons/hicolor/128x128/apps/logisim-evolution.png"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/logisim-evolution.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Logisim-evolution
GenericName=Logic Simulator
Comment=Design and simulate digital logic circuits, and export them to FPGAs
Exec=logisim-evolution %f
Icon=logisim-evolution
Terminal=false
StartupWMClass=com-cburch-logisim-Main
Categories=Education;Electronics;Engineering;
Keywords=logic;circuit;simulator;gate;fpga;vhdl;verilog;cpu;
DESKTOP
chmod 644 "$PKG/usr/share/applications/logisim-evolution.desktop"
