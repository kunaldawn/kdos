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


# The INSTALL_*DEPENDENCIES switches copy the Qt and library files a vcpkg
# build links into the package; on a system build they would install second
# copies of other ports' libraries. The scanner plugin compiles its SANE
# backend only when libsane is found, and is otherwise an empty entry in the
# editor's menu. Every translation catalogue is installed whatever the build
# is told, so all but English are removed from the package: bundled data is
# English only.
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

find "$PKG/usr/share/pdf4qt/translations" -name '*.qm' ! -name '*_en.qm' -delete

# Each window's app_id is its executable's name, and each entry names it in
# StartupWMClass, or the taskbar cannot match the launch pad's window, whose
# file id ends in Pdf4qt, to its Start menu row. A file reaches a program only
# through a field code in Exec, and upstream's entries carry none, so opening
# a PDF with any of them would start an empty window: the viewer and the
# editor take one file, the diff tool takes two. The page master and the
# launch pad read no file from their command line, so they claim no type and
# stay out of the Open With list.
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
Categories=Office;
StartupWMClass=Pdf4QtLaunchPad
EOF
