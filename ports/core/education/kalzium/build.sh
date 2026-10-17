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

export LDFLAGS="$LDFLAGS -Wl,--strip-debug"

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_OpenBabel3=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_AvogadroLibs=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Eigen3=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_OCaml=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Libfacile=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

cat > "$PKG/usr/share/applications/org.kde.kalzium.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Kalzium
GenericName=Periodic Table of Elements
Comment=Periodic table of the elements
TryExec=kalzium
Exec=kalzium
Icon=kalzium
Terminal=false
StartupWMClass=org.kde.kalzium
X-DocPath=kalzium/index.html
Categories=Qt;KDE;Education;Science;Chemistry;
Keywords=periodic;table;element;chemistry;molar;mass;isotope;kalzium;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kalzium.desktop"
cat > "$PKG/usr/share/applications/org.kde.kalzium_cml.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Kalzium
Exec=kalzium --molecule %f
Icon=kalzium
Terminal=false
NoDisplay=true
StartupWMClass=org.kde.kalzium
MimeType=chemical/x-cml;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kalzium_cml.desktop"
