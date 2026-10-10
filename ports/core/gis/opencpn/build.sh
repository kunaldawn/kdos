# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# musl declares select(2) in <sys/select.h> only. system-shapelib drops the
# USE_CPL that dychart.h defines for every source: it makes the system
# shapefil.h include the system GDAL's cpl_conv.h on top of the CPL headers
# OpenCPN bundles, and the two sets do not compile together. openssl4 builds
# the self-signed certificate's name separately and sets it as subject and
# issuer: OpenSSL 4 returns the certificate's own subject name const.
patch -p1 -i "$PORT_SRC/mdns.patch"
patch -p1 -i "$PORT_SRC/system-shapelib.patch"
patch -p1 -i "$PORT_SRC/openssl4.patch"

# System libraries wherever a port exists; the bundled copies (jasper,
# wxcurl, unarr) fill the rest, and nothing is fetched. The minimal GSHHS
# coastline, the tide and current harmonics and the manual ship in the package.
# The target tuple names the platform plugins are matched against; without it
# the build asks lsb_release, which this system does not have.
# GLEW's CMake package is ignored under both /usr/lib and /lib, which is the
# same directory, so FindGLEW searches for the library itself: it reads a
# shared GLEW's path from the package only as a Windows import library, and
# GLEW_LIBRARY would be left NOTFOUND.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DOCPN_USE_BUNDLED_LIBS=OFF \
	-DOCPN_BUNDLE_GSHHS=ON \
	-DOCPN_BUNDLE_TCDATA=ON \
	-DOCPN_BUNDLE_DOCS=ON \
	-DOCPN_USE_GL=ON \
	-DOCPN_USE_CURL=ON \
	-DOCPN_USE_WEBVIEW=OFF \
	-DOCPN_ENABLE_PORTAUDIO=ON \
	-DOCPN_ENABLE_SNDFILE=ON \
	-DOCPN_USE_UDEV_PORTS=ON \
	-DOCPN_BUILD_TEST=OFF \
	-DOCPN_CI_BUILD=OFF \
	-DFETCHCONTENT_FULLY_DISCONNECTED=ON \
	-DCMAKE_IGNORE_PATH='/usr/lib/cmake/glew;/lib/cmake/glew' \
	"-DOCPN_TARGET_TUPLE=kdos;1;$(uname -m)"
cmake --build build
DESTDIR=$PKG cmake --install build

# English only: the interface and plugin catalogues of every other language go.
find "$PKG/usr/share/locale" -mindepth 1 -maxdepth 1 ! -name 'en*' -exec rm -rf {} +

# OpenCPN sets GDK_BACKEND=x11 for itself under Wayland, so it is an X11
# window whose class is the program name.
cat > "$PKG/usr/share/applications/opencpn.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=OpenCPN
GenericName=Chart Plotter
Comment=Navigate with electronic charts, GPS and AIS
Exec=opencpn
Icon=opencpn
Terminal=false
StartupWMClass=opencpn
Categories=Education;Science;Geography;Maps;
Keywords=gps;navigation;chart;nautical;marine;boat;ais;enc;
DESKTOP
chmod 644 "$PKG/usr/share/applications/opencpn.desktop"
