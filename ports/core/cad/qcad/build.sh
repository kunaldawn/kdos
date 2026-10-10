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

# Every action in QCAD is ECMAScript run by QJSEngine. Under Qt 6 the script
# bindings are not in this repository: qtjsapi (the Qt API) and qcadjsapi (the
# QCAD API) are sibling projects of the same tag, later sources here. Without
# them the program starts with no menus. Both find the main tree as ../qcad,
# qcadjsapi finds qtjsapi's headers as ../qtjsapi, and both write their
# libraries into the main tree's release/ and plugins/ directories.
ln -s "$SRC" "$SRC_ROOT/qcad"
ln -s "$SRC_ROOT/qtjsapi-$version" "$SRC_ROOT/qtjsapi"

# OpenNURBS calls fcloseall, which musl does not have; musl-fcloseall takes the
# branch it already has for the platforms without it, which reports EOF.
# RDebug::printBacktrace uses <execinfo.h>, which musl does not have either;
# no-execinfo takes its Windows branch, which prints nothing, where the header
# is missing.
patch -p1 -i "$PORT_SRC/musl-fcloseall.patch"
patch -p1 -i "$PORT_SRC/no-execinfo.patch"

# The build writes release/ (the program and its libraries) and plugins/
# inside the source tree and has no install rules. Run paths are skipped:
# the build would record its own tree in every library; the launcher below
# names the library directory instead. dxflib, opennurbs (with its private
# zlib and FreeType) and the spatial index are linked in statically from
# src/3rdparty.
cmake -S . -B build -G Ninja -Wno-dev -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_SKIP_RPATH=ON \
	-DBUILD_QT6=ON
ninja -C build

# The optional Qt modules qtjsapi probes (Core5Compat, PrintSupport,
# QuickWidgets, Sql, Positioning, Location) are all in depends, so the
# binding set does not follow build order.
for p in qtjsapi qcadjsapi; do
	cmake -S "$SRC_ROOT/$p-$version" -B "$SRC_ROOT/$p-build" -G Ninja -Wno-dev \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_SKIP_RPATH=ON
	ninja -C "$SRC_ROOT/$p-build"
done

# QCAD finds its plugins, fonts, patterns, line types, part library and
# themes beside its executable, so the program tree is /usr/lib/qcad. The
# scripts are compiled into the qcadscripts plugin.
_dest="$PKG/usr/lib/qcad"
install -d "$_dest/plugins"
install -m755 release/qcad-bin "$_dest/qcad-bin"
cp -P release/lib*.so* "$_dest/"
cp -P plugins/lib*.so* "$_dest/plugins/"
cp -r fonts patterns linetypes libraries themes examples "$_dest/"
chmod -R u=rwX,go=rX "$_dest"

# fonts/qt holds DejaVu and Vera copies for a machine with no system fonts;
# QCAD registers every TTF under fonts/ as an application font, so shipped
# they would be second copies of families the system already has.
rm -rf "$_dest/fonts/qt"

install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/qcad" <<'KDOS_SH'
#!/bin/sh
LD_LIBRARY_PATH=/usr/lib/qcad${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH} \
	exec /usr/lib/qcad/qcad-bin "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/qcad"

install -Dm644 qcad.1 "$PKG/usr/share/man/man1/qcad.1"
install -Dm644 scripts/org.qcad.QCAD.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/org.qcad.QCAD.png"

# UPSTREAM'S ENTRY IS REPLACED: its StartupWMClass is the X11 class. main()
# sets the organisation domain QCAD.org and no desktop file name, so Qt
# builds the Wayland app_id from the reversed domain and the program name,
# org.QCAD.qcad-bin. It claims no MimeType: DXF opens in LibreCAD, and QCAD
# is chosen from the file manager's Open With.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/org.qcad.QCAD.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=QCAD
GenericName=2D CAD
Comment=Technical 2D drawings, plans and parts in DXF
Exec=qcad %F
Icon=org.qcad.QCAD
Terminal=false
StartupWMClass=org.QCAD.qcad-bin
Categories=Graphics;VectorGraphics;Engineering;Qt;
Keywords=cad;2d;drafting;drawing;dxf;dwg;plan;qcad;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.qcad.QCAD.desktop"
