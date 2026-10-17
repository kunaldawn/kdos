# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

unzip -q "$name-$version.zip"
cd "librepcb-$version"

patch -p1 -i "$PORT_SRC/offline-defaults.patch"

tar xf "$PORT_SRC/$name-vendor-$version.tar.xz" -C "$SRC_ROOT"
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DBUILD_TESTS=OFF \
	-DBUILD_DISALLOW_WARNINGS=OFF \
	-DUSE_GLU=ON \
	-DUSE_OPENCASCADE=ON \
	-DUNBUNDLE_POLYCLIPPING=ON
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share/librepcb/i18n" -name '*.qm' ! -name '*_en*.qm' -delete

cat > "$PKG/usr/share/applications/org.librepcb.LibrePCB.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=LibrePCB
GenericName=PCB Designer
Comment=Draw schematics and lay out printed circuit boards
Exec=librepcb %U
Icon=org.librepcb.LibrePCB
Terminal=false
StartupWMClass=org.librepcb.LibrePCB
Categories=Development;Engineering;Electronics;
MimeType=application/x-librepcb-project;application/x-librepcb-project-archive;
Keywords=pcb;schematic;eda;electronics;circuit;gerber;layout;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.librepcb.LibrePCB.desktop"
