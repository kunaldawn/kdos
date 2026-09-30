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

# ARCH_OPT is passed straight to -march, and upstream's default, native, ties
# the binary to the build machine's processor. It names the architecture's
# baseline here.
case "$(uname -m)" in
	aarch64) _march=armv8-a ;;
	*) _march=x86-64 ;;
esac

# ENABLE_EXTERNAL_LIBRARIES=OFF: the ON and AUTO modes clone and build the
# helper libraries from git at configure time. Each optional library is then
# found from its port: codec2 (FreeDV, M17), FFmpeg (DATV receive), Opus,
# FLAC, zlib, hidapi (FUNcube), cm256cc (remote input, output, sink and
# source), dsdcc and mbelib (DSD), SerialDV (AMBE), libdab and FAAD (DAB),
# sgp4 (satellite tracker), aptdec (APT), cspice (star tracker), ggmorse
# (Morse decoder), libinmarsatc (Inmarsat-C), RNNoise (denoiser), libsigmf
# (SigMF file input and sink) and OpenCV (ATV and DATV transmit). cspice keeps its headers in
# /usr/include/cspice, which the bundled find module does not search, so the
# directory is named. libsigmf's headers include FlatBuffers' own, which take
# _XOPEN_VERSION 700 to mean strtoll_l and strtoull_l exist; musl has
# neither. The CUDA VkFFT engine is never looked up, and neither is
# QtWebEngine: it serves only the sky map, which draws from online services.
# The four remote plugins also need SSE3 or NEON at compile time; ARCH_OPT's
# x86-64 baseline defines no SSE3, so they are built on aarch64 only.
#
# Radios by library: Airspy, Airspy HF+, bladeRF, HackRF, RTL-SDR, LimeSuite,
# PlutoSDR (libiio, on its 0.x buffer API), MiriSDR, Perseus, USRP (UHD),
# SoapySDR and FUNcube; Fobos loads libfobos at run time; Metis and the
# Aaronia RTSA speak the network. SDRplay and XTRX need libraries that are
# not ports. The benchmark tool is not built; sdrangelsrv, the headless
# server, is.
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
	-DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Qt6WebEngineCore=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Qt6WebEngineQuick=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_Qt6WebEngineWidgets=ON \
	-DCCACHE=OFF \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install

# Upstream's entry has no StartupWMClass. With no organisation domain set,
# Qt's Wayland app_id is the executable's name. The panel reads PNG only.
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
# One plugin per optional library: a library the configure stops finding
# fails the build here instead of dropping its plugin in silence.
for _p in inputairspyhf inputlimesdr inputplutosdr inputsdrplay inputperseus \
	inputusrp inputFOBOS demoddsd demoddab demodapt demodinmarsat \
	sigmffilesink featureambe featuremorsedecoder featuredenoiser \
	featurestartracker featurelimerfe modatv; do
	test -n "$(find "$PKG/usr/lib/sdrangel/plugins" -name "*$_p.so" | head -1)"
done
