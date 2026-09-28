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

# fmt and LibRaw are the ports. Left to their defaults, fmt is cloned from
# GitHub at configure time, and LibRaw is built from the copy in the tarball.
# PROC_TARGET_NUMBER=0 adds no -march or -mtune, so the package runs on every
# x86-64 machine. libcanberra is the queue-finished sound; JPEG XL is named on
# rather than probed. The bundled KLT feature tracker has no port and stays.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CACHE_NAME_SUFFIX="" \
	-D BUILD_BUNDLE=OFF \
	-D BUILD_SHARED=OFF \
	-D PROC_TARGET_NUMBER=0 \
	-D OPTION_OMP=ON \
	-D WITH_LTO=OFF \
	-D WITH_SYSTEM_FMT=ON \
	-D WITH_SYSTEM_LIBRAW=ON \
	-D WITH_SYSTEM_KLT=OFF \
	-D WITH_SIMDE=OFF \
	-D WITH_JXL=ON \
	-D SVG_BACKEND=librsvg \
	-D USE_LIBCANBERRA=ON \
	-D WARNINGS_AS_ERRORS=OFF \
	-D ENABLE_TCMALLOC=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# Bundled data is English only: "default" is the built-in English, and the
# two English variants stay beside it.
find "$PKG/usr/share/rawtherapee/languages" -type f ! -name default \
	! -name 'English (UK)' ! -name 'English (US)' ! -name LICENSE ! -name README \
	-exec rm -f {} +

# UPSTREAM'S ENTRY IS REPLACED to leave inode/directory out of MimeType:
# claimed here, a folder could open in RawTherapee instead of the file
# manager. GTK 3 takes the Wayland app_id from the program name, which
# gtk_init sets from argv[0], so StartupWMClass is "rawtherapee", not the
# Gtk::Application id. The hicolor PNGs it names are upstream's, installed
# above.
cat > "$PKG/usr/share/applications/rawtherapee.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=RawTherapee
GenericName=Raw Photo Editor
Comment=An advanced raw photo development program
TryExec=rawtherapee
Exec=rawtherapee %f
Icon=rawtherapee
Terminal=false
StartupNotify=true
StartupWMClass=rawtherapee
MimeType=image/jpeg;image/png;image/tiff;image/x-adobe-dng;image/x-canon-cr2;image/x-canon-cr3;image/x-canon-crf;image/x-canon-crw;image/x-fuji-raf;image/x-hasselblad-3fr;image/x-hasselblad-fff;image/x-jpg;image/x-kodak-dcr;image/x-kodak-k25;image/x-kodak-kdc;image/x-leaf-mos;image/x-leica-rwl;image/x-mamiya-mef;image/x-minolta-mrw;image/x-nikon-nef;image/x-nikon-nrw;image/x-olympus-orf;image/x-panasonic-raw;image/x-panasonic-rw2;image/x-pentax-pef;image/x-pentax-raw;image/x-phaseone-iiq;image/x-raw;image/x-rwz;image/x-samsung-srw;image/x-sigma-x3f;image/x-sony-arq;image/x-sony-arw;image/x-sony-sr2;image/x-sony-srf;image/x-tif;
Categories=Graphics;Photography;2DGraphics;RasterGraphics;GTK;
Keywords=raw;photo;photography;develop;pp3;graphics;rawtherapee;
DESKTOP
chmod 644 "$PKG/usr/share/applications/rawtherapee.desktop"
