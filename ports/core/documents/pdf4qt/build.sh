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

patch -p1 -i "$PORT_SRC/openssl4.patch"

cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D PDF4QT_BUILD_TESTS=OFF \
	-D PDF4QT_INSTALL_DEPENDENCIES=OFF \
	-D PDF4QT_INSTALL_QT_DEPENDENCIES=OFF \
	-D PDF4QT_INSTALL_TO_USR=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

_apps="$PKG/usr/share/applications"
_entry() {
	cat > "$_apps/io.github.JakubMelka.Pdf4qt.$1.desktop" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=$2
Comment=$3
Exec=$1${4:+ $4}
Icon=io.github.JakubMelka.Pdf4qt.$1
Terminal=false
Categories=Office;
${4:+MimeType=application/pdf;
}StartupWMClass=$1
EOF
}
_entry Pdf4QtViewer 'PDF4QT Viewer' 'View and navigate PDF documents' %f
_entry Pdf4QtEditor 'PDF4QT Editor' 'Edit and modify PDF files' %f
_entry Pdf4QtPageMaster 'PDF4QT PageMaster' \
	'Organize, merge, split, and duplicate pages in PDF files'
_entry Pdf4QtDiff 'PDF4QT Diff' 'Compare PDF documents' %F
cat > "$_apps/io.github.JakubMelka.Pdf4qt.desktop" <<'EOF'
[Desktop Entry]
Version=1.0
Type=Application
Name=PDF4QT LaunchPad
Comment=Launch PDF applications
Exec=Pdf4QtLaunchPad
Icon=io.github.JakubMelka.Pdf4qt
Terminal=false
NoDisplay=true
Categories=Office;
StartupWMClass=Pdf4QtLaunchPad
EOF
