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

ln -s "$SRC" "$SRC_ROOT/qcad"
ln -s "$SRC_ROOT/qtjsapi-$version" "$SRC_ROOT/qtjsapi"

patch -p1 -i "$PORT_SRC/musl-fcloseall.patch"
patch -p1 -i "$PORT_SRC/no-execinfo.patch"

cmake -S . -B build -G Ninja -Wno-dev -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_SKIP_RPATH=ON \
	-DBUILD_QT6=ON
ninja -C build

for p in qtjsapi qcadjsapi; do
	cmake -S "$SRC_ROOT/$p-$version" -B "$SRC_ROOT/$p-build" -G Ninja -Wno-dev \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_SKIP_RPATH=ON
	ninja -C "$SRC_ROOT/$p-build"
done

_dest="$PKG/usr/lib/qcad"
install -d "$_dest/plugins"
install -m755 release/qcad-bin "$_dest/qcad-bin"
cp -P release/lib*.so* "$_dest/"
cp -P plugins/lib*.so* "$_dest/plugins/"
cp -r fonts patterns linetypes libraries themes examples "$_dest/"
chmod -R u=rwX,go=rX "$_dest"

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
