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

# Every optional part is built. The 3D molecule editor (compoundviewer) is
# built only when Open Babel, AvogadroLibs and Eigen are all found, and then
# pulls in KNewStuff and Qt's OpenGL module as required. The equation solver
# is OCaml: FindLibfacile looks for facile.cmxa under `ocamlc -where`, and
# ocamlopt -output-obj links the solver and the OCaml runtime into kalzium.
# Each of those finds is optional upstream and would drop its feature
# silently; CMAKE_REQUIRE_FIND_PACKAGE_* makes a missing one fail configure.
# KF_SKIP_PO_PROCESSING leaves the translation catalogues out: bundled data
# is English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
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

# kdoctools_install() builds every translated handbook and manual page; only
# the English ones are kept.
for d in "$PKG"/usr/share/doc/HTML/*/ "$PKG"/usr/share/man/*/; do
	case ${d%/} in
	*/HTML/en | */man/man[0-9]*) ;;
	*) rm -rf "$d" ;;
	esac
done

# Both entries are replaced. The main one for StartupWMClass: KAboutData
# makes the Wayland app_id org.kde.kalzium. The chemical/x-cml handler, which
# opens a file in the molecule editor, because upstream's Exec writes
# `-molecule %f`: QCommandLineParser reads that as -m with the value
# "olecule", and the file is never opened.
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
