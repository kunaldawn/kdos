# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/mdns.patch"
patch -p1 -i "$PORT_SRC/system-shapelib.patch"
patch -p1 -i "$PORT_SRC/openssl4.patch"
patch -p1 -i "$PORT_SRC/udev-rule-destdir.patch"

export LDFLAGS="$LDFLAGS -Wl,--strip-debug"

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
	-DCMAKE_IGNORE_PATH='/usr/lib/cmake/glew;/usr/lib64/cmake/glew;/lib/cmake/glew;/lib64/cmake/glew' \
	"-DOCPN_TARGET_TUPLE=kdos;1;$(uname -m)"
cmake --build build
DESTDIR=$PKG cmake --install build

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

test -f "$PKG/usr/lib/udev/rules.d/98-sglock-ocpn.rules"
