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

patch -p1 -i "$PORT_SRC/sgp4-range-rate.patch"

case "$(uname -m)" in
	aarch64) _march=armv8-a ;;
	*) _march=x86-64 ;;
esac

export CXXFLAGS="$CXXFLAGS -DFLATBUFFERS_LOCALE_INDEPENDENT=0"
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_POLICY_DEFAULT_CMP0167=NEW \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DARCH_OPT=$_march \
	-DENABLE_QT6=ON \
	-DENABLE_EXTERNAL_LIBRARIES=OFF \
	-DENABLE_LIBUNWIND=ON \
	-DDEBUG_OUTPUT=OFF \
	-DBUILD_GUI=ON \
	-DBUILD_SERVER=ON \
	-DBUILD_BENCH=OFF \
	-DBUNDLE=OFF \
	-DENABLE_PROFILER=OFF \
	-DRX_SAMPLE_24BIT=ON \
	-DVKFFT_BACKEND=1 \
	-DENABLE_AIRSPY=ON \
	-DENABLE_BLADERF=ON \
	-DENABLE_HACKRF=ON \
	-DENABLE_RTLSDR=ON \
	-DENABLE_SOAPYSDR=ON \
	-DENABLE_FUNCUBE=ON \
	-DENABLE_METIS=ON \
	-DENABLE_AARONIARTSA=ON \
	-DENABLE_AIRSPYHF=ON \
	-DENABLE_LIMESUITE=ON \
	-DENABLE_IIO=ON \
	-DENABLE_MIRISDR=ON \
	-DENABLE_PERSEUS=ON \
	-DENABLE_SDRPLAY=OFF \
	-DENABLE_XTRX=OFF \
	-DENABLE_USRP=ON \
	-DENABLE_FOBOS=ON \
	-DENABLE_PACK_MIRSDRAPI=OFF \
	-DCSPICE_INCLUDE_DIR=/usr/include/cspice \
	-DCM256CC_DIR=/usr \
	-DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Qt6WebEngineCore=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Qt6WebEngineQuick=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Qt6WebEngineWidgets=ON \
	-DCCACHE=OFF \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install

for s in 48 64 128 256 512; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s cmake/cpack/sdrangel_icon.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/sdrangel_icon.png"
done
cat > "$PKG/usr/share/applications/sdrangel.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=SDRangel
GenericName=SDR Receiver, Transmitter and Analyser
Comment=Receive, transmit and decode radio: ADS-B, AIS, APRS, FT8, DATV, FreeDV and more
Exec=sdrangel
Terminal=false
Icon=sdrangel_icon
Categories=Network;HamRadio;Qt;
Keywords=SDR;radio;transceiver;ADS-B;AIS;APRS;FT8;DATV;FreeDV;
StartupWMClass=sdrangel
DESKTOP
chmod 644 "$PKG/usr/share/applications/sdrangel.desktop"

test -x "$PKG/usr/bin/sdrangel"
test -x "$PKG/usr/bin/sdrangelsrv"
test -n "$(find "$PKG/usr/lib/sdrangel/plugins" -name '*inputrtlsdr.so' | head -1)"
for _p in inputairspyhf inputlimesdr inputplutosdr inputsdrplay inputperseus \
	inputusrp inputFOBOS demoddsd demoddab demodapt demodinmarsat \
	sigmffilesink featureambe featuremorsedecoder featuredenoiser \
	featurestartracker featurelimerfe modatv; do
	test -n "$(find "$PKG/usr/lib/sdrangel/plugins" -name "*$_p.so" | head -1)"
done
