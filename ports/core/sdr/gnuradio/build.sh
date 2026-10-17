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

_pyinc=$(python3 -c 'import sysconfig; print(sysconfig.get_path("include"))')
_pylib=$(python3 -c 'import sysconfig; print(sysconfig.get_config_var("LIBDIR") + "/" + sysconfig.get_config_var("LDLIBRARY"))')
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_POLICY_DEFAULT_CMP0167=NEW \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DPYTHON_EXECUTABLE=/usr/bin/python3 \
	-DPYTHON_INCLUDE_DIR="$_pyinc" \
	-DPYTHON_LIBRARY="$_pylib" \
	-Dpybind11_DIR="$(python3 -m pybind11 --cmakedir)" \
	-DENABLE_DEFAULT=OFF \
	-DENABLE_TESTING=OFF \
	-DENABLE_EXAMPLES=ON \
	-DENABLE_NATIVE=OFF \
	-DENABLE_PYTHON=ON \
	-DENABLE_POSTINSTALL=OFF \
	-DENABLE_DOXYGEN=OFF \
	-DENABLE_MANPAGES=ON \
	-DENABLE_GNURADIO_RUNTIME=ON \
	-DENABLE_COMMON_PCH=OFF \
	-DENABLE_GR_CTRLPORT=ON \
	-DENABLE_CTRLPORT_THRIFT=ON \
	-DENABLE_GRC=ON \
	-DGRC_XTERM_EXE=/usr/bin/foot \
	-DENABLE_JSONYAML_BLOCKS=ON \
	-DENABLE_GR_BLOCKS=ON \
	-DENABLE_GR_FEC=ON \
	-DENABLE_GR_FFT=ON \
	-DENABLE_GR_FILTER=ON \
	-DENABLE_GR_ANALOG=ON \
	-DENABLE_GR_DIGITAL=ON \
	-DENABLE_GR_DTV=ON \
	-DENABLE_GR_AUDIO=ON \
	-DENABLE_GR_CHANNELS=ON \
	-DENABLE_GR_PDU=ON \
	-DENABLE_GR_TRELLIS=ON \
	-DENABLE_GR_UTILS=ON \
	-DENABLE_GR_MODTOOL=ON \
	-DENABLE_GR_BLOCKTOOL=ON \
	-DENABLE_GR_VOCODER=ON \
	-DENABLE_GR_WAVELET=ON \
	-DENABLE_GR_NETWORK=ON \
	-DENABLE_GR_SOAPY=ON \
	-DENABLE_GR_QTGUI=ON \
	-DENABLE_GR_UHD=ON \
	-DENABLE_UHD_RFNOC=ON \
	-DENABLE_GR_IIO=ON \
	-DENABLE_GR_ZEROMQ=ON \
	-DENABLE_GR_VIDEO_SDL=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_JACK=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_OSS=ON \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install
for m in "$PKG"/usr/share/man/man1/*.1; do
	[ -e "$PKG/usr/bin/$(basename "$m" .1)" ] || rm -f "$m"
done
_fd=grc/scripts/freedesktop
install -Dm644 $_fd/gnuradio-grc.desktop \
	"$PKG/usr/share/applications/gnuradio-grc.desktop"
install -Dm644 $_fd/gnuradio-grc.xml \
	"$PKG/usr/share/mime/packages/gnuradio-grc.xml"
install -Dm644 $_fd/org.gnuradio.grc.metainfo.xml \
	"$PKG/usr/share/metainfo/org.gnuradio.grc.metainfo.xml"
for s in 16 24 32 48 64 128 256; do
	install -Dm644 $_fd/grc-icon-$s.png \
		"$PKG/usr/share/icons/hicolor/${s}x${s}/apps/gnuradio-grc.png"
	install -Dm644 $_fd/grc-icon-$s.png \
		"$PKG/usr/share/icons/hicolor/${s}x${s}/mimetypes/application-gnuradio-grc.png"
done
_site=$(python3 -c 'import sysconfig; print(sysconfig.get_path("purelib"))')
install -d "$PKG$_site"
printf 'from PyQt5.sip import *\n' > "$PKG$_site/sip.py"
chmod 644 "$PKG$_site/sip.py"
test -x "$PKG/usr/bin/gnuradio-companion"
grep -qx 'xterm_executable = /usr/bin/foot' "$PKG/etc/gnuradio/conf.d/grc.conf"
test -n "$(find "$PKG"/usr/lib/python3*/site-packages/gnuradio/gr -name '*.so' | head -1)"
test -n "$(find "$PKG"/usr/lib/python3*/site-packages/gnuradio/qtgui -name '*.so' | head -1)"
