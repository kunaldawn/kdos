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

# ONE HYBRID LIBRARY, WAYLAND FIRST. With both backends on, FLTK picks Wayland
# at run time whenever a Wayland display is reachable and X11 otherwise;
# FLTK_BACKEND=x11 in the environment forces Xwayland. A hybrid build draws
# every X11 window through Cairo and lays out text with Pango, so the X11 half
# needs cairo-xlib and pangoxft: with either missing, configure stops.
#
# THE X11 EXTENSIONS ARE MANDATORY IN A HYBRID BUILD. Xfixes, Xrender, Xft,
# Xcursor and Xinerama each stop configure when absent, rather than switching
# a feature off.
#
# libdecor is the system's: with FLTK_USE_SYSTEM_LIBDECOR the bundled copy is
# not compiled, and window frames come from the plugins the libdecor port
# installs. Its cursor-theme lookup reads the desktop's settings over D-Bus.
#
# OpenGL is EGL on Wayland and GLX on X11, and configure requires gl.pc, egl.pc,
# glu.pc and wayland-egl.pc for the GL library to be built at all.
#
# Static archives are always built; FLTK_BUILD_SHARED_LIBS adds the shared
# libraries a program links by default.
cmake -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
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

# UPSTREAM'S ENTRIES ARE REPLACED to add StartupWMClass and a distinct Name.
# show(argc, argv) sets the window class, and so the Wayland app_id, from the
# program's basename. The hicolor PNGs and the MIME XML the entries name are
# upstream's, installed above.
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
