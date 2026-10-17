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

CFLAGS= CXXFLAGS= cmake -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_FLAGS_RELEASE="$CFLAGS -O3 -DNDEBUG" \
	-DCMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS -O3 -DNDEBUG" \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DFLTK_BUILD_SHARED_LIBS=ON \
	-DFLTK_BACKEND_WAYLAND=ON \
	-DFLTK_BACKEND_X11=ON \
	-DFLTK_USE_SYSTEM_LIBDECOR=ON \
	-DFLTK_USE_DBUS=ON \
	-DFLTK_USE_SYSTEM_LIBJPEG=ON \
	-DFLTK_USE_SYSTEM_LIBPNG=ON \
	-DFLTK_USE_SYSTEM_ZLIB=ON \
	-DFLTK_USE_PTHREADS=ON \
	-DFLTK_BUILD_GL=ON \
	-DFLTK_OPTION_SVG=ON \
	-DFLTK_OPTION_PRINT_SUPPORT=ON \
	-DFLTK_OPTION_FILESYSTEM_SUPPORT=ON \
	-DFLTK_OPTION_LARGE_FILE=ON \
	-DFLTK_OPTION_CAIRO_WINDOW=ON \
	-DFLTK_OPTION_CAIRO_EXT=ON \
	-DFLTK_BUILD_FORMS=ON \
	-DFLTK_BUILD_FLUID=ON \
	-DFLTK_BUILD_FLTK_OPTIONS=ON \
	-DFLTK_BUILD_TEST=OFF \
	-DFLTK_BUILD_EXAMPLES=OFF \
	-DFLTK_BUILD_HTML_DOCS=OFF \
	-DFLTK_BUILD_PDF_DOCS=OFF \
	-DFLTK_BUILD_FLUID_DOCS=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/fluid.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=FLUID
GenericName=GUI Designer
Comment=Design FLTK user interfaces and generate their C++ code
TryExec=fluid
Exec=fluid %F
Icon=fluid
Terminal=false
StartupWMClass=fluid
MimeType=application/x-fluid;
Categories=Development;GUIDesigner;
Keywords=fltk;gui;designer;interface;fluid;
EOF
cat > "$PKG/usr/share/applications/fltk-options.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=FLTK Options
GenericName=Toolkit Settings
Comment=System and user options for FLTK programs
TryExec=fltk-options
Exec=fltk-options %F
Icon=fltk-options
Terminal=false
StartupWMClass=fltk-options
MimeType=application/x-fltk-options;
Categories=Development;Settings;
Keywords=fltk;options;settings;toolkit;
EOF
chmod 644 "$PKG/usr/share/applications/fluid.desktop" \
	"$PKG/usr/share/applications/fltk-options.desktop"
