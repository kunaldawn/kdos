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

# no-graphite drops loop-nest-optimize, tree-loop-linear, loop-block and
# loop-strip-mine from the sources' #pragma GCC optimize lists: they are
# Graphite passes, this GCC is built without isl, and it refuses each one as
# unimplemented. A pragma outranks the command line, so no flag answers it.
patch -p1 -i "$PORT_SRC/no-graphite.patch"

# BINARY_PACKAGE_BUILD compiles for the generic x86-64 target instead of the
# builder's CPU, and so does the bundled rawspeed, which reads the same flag;
# without it the package faults on an older processor.
#
# Offline and no secrets service: the map view needs online tiles (and
# osm-gps-map, not a port), the AI modules download their models, and the
# KWallet and libsecret back-ends only keep web-export credentials, so all
# four are off. colord, G'MIC, GraphicsMagick and libunity are not ports.
# ImageMagick 7 is the fallback loader for FITS, GIF, JPEG 2000 codestreams
# and the rest. Lua is the lua54 port: darktable takes Lua 5.4 only, and
# DONT_USE_INTERNAL_LUA fails the configure rather than falling back to the
# copy in the tarball. LibRaw is the port too, required so the bundled copy is
# never built. The OpenCL loader is opened at run time and needs nothing
# here; test-compiling the kernels would need clang and is off. cmstest is an
# X11 colour-profile probe with nothing to report under Wayland.
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_TESTING=OFF \
	-D BINARY_PACKAGE_BUILD=ON \
	-D RAWSPEED_ENABLE_LTO=OFF \
	-D RAWSPEED_ENABLE_WERROR=OFF \
	-D USE_CAMERA_SUPPORT=ON \
	-D USE_COLORD=OFF \
	-D USE_MAP=OFF \
	-D USE_LUA=ON \
	-D DONT_USE_INTERNAL_LUA=ON \
	-D USE_KWALLET=OFF \
	-D USE_LIBSECRET=OFF \
	-D USE_UNITY=OFF \
	-D USE_OPENMP=ON \
	-D USE_OPENCL=ON \
	-D TESTBUILD_OPENCL_PROGRAMS=OFF \
	-D USE_GRAPHICSMAGICK=OFF \
	-D USE_IMAGEMAGICK=ON \
	-D USE_XMLLINT=ON \
	-D USE_PORTMIDI=ON \
	-D USE_OPENJPEG=ON \
	-D USE_JXL=ON \
	-D USE_WEBP=ON \
	-D USE_AVIF=ON \
	-D USE_HEIF=ON \
	-D USE_XCF=ON \
	-D USE_ISOBMFF=ON \
	-D USE_LIBRAW=ON \
	-D DONT_USE_INTERNAL_LIBRAW=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_libraw=ON \
	-D USE_AI=OFF \
	-D USE_OPENEXR=ON \
	-D USE_GMIC=OFF \
	-D USE_ICU=ON \
	-D USE_SDL2=ON \
	-D BUILD_PRINT=ON \
	-D BUILD_CMSTEST=OFF \
	-D BUILD_RS_IDENTIFY=ON \
	-D VALIDATE_APPDATA_FILE=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# The catalogues are built unconditionally; bundled data is English only.
rm -rf "$PKG/usr/share/locale"
find "$PKG/usr/share/man" -mindepth 1 -maxdepth 1 ! -name 'man[0-9]*' -exec rm -rf {} +

# UPSTREAM'S ENTRY IS REPLACED. darktable sets its program name, and so the
# Wayland app_id, to org.darktable.darktable, which StartupWMClass has to
# name, and upstream's Exec is an absolute path. The hicolor PNGs it names
# are upstream's, installed above.
cat > "$PKG/usr/share/applications/org.darktable.darktable.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Darktable
GenericName=Virtual Lighttable and Darkroom
Comment=Organize and develop images from digital cameras
TryExec=darktable
Exec=darktable %U
Icon=darktable
Terminal=false
StartupNotify=true
StartupWMClass=org.darktable.darktable
MimeType=application/x-darktable;image/x-dcraw;image/x-adobe-dng;image/x-canon-cr2;image/x-canon-cr3;image/x-canon-crw;image/x-fuji-raf;image/x-kodak-dcr;image/x-kodak-kdc;image/x-minolta-mrw;image/x-nikon-nef;image/x-nikon-nrw;image/x-olympus-orf;image/x-panasonic-rw;image/x-panasonic-rw2;image/x-pentax-pef;image/x-sony-arw;image/x-sony-sr2;image/x-sony-srf;image/jpeg;image/png;image/tiff;image/x-portable-bitmap;image/x-portable-graymap;image/x-portable-pixmap;image/x-portable-floatmap;image/vnd.radiance;image/avif;image/x-exr;image/aces;image/heif;image/heic;image/jp2;image/jxl;image/webp;image/qoi;image/fits;
Categories=Graphics;Photography;GTK;
Keywords=graphics;photography;raw;darkroom;lighttable;darktable;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.darktable.darktable.desktop"
