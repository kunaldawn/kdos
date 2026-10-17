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

patch -p1 -i "$PORT_SRC/satdump-app-id.patch"

patch -p1 -i "$PORT_SRC/satdump-dvb-generic.patch"

patch -p1 -i "$PORT_SRC/satdump-ctime.patch"

patch -p1 -i "$PORT_SRC/satdump-calibration-constants.patch"

export CXXFLAGS="$CXXFLAGS -Wno-template-body -DSOL_NO_LUA_HPP=1"

cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_GUI=ON \
	-DBUILD_GLES=OFF \
	-DBUILD_TESTING=OFF \
	-DBUILD_TOOLS=OFF \
	-DBUILD_ZIQ=ON \
	-DBUILD_ZIQ2=OFF \
	-DBUILD_OPENCL=ON \
	-DBUILD_OPENMP=ON \
	-DBUILD_DOCS=OFF \
	-DENABLE_INSTALL=ON \
	-DPLUGINS_ALL=ON \
	-DPLUGIN_SCRIPTING=OFF \
	-DPLUGIN_BOCHUM_SUPPORT=OFF \
	-DPLUGIN_BITVIEW_APP=OFF \
	-DPLUGIN_GVAR_EXTENDED=OFF \
	-DPLUGIN_RTLSDR_SDR_SUPPORT=ON \
	-DPLUGIN_AIRSPY_SDR_SUPPORT=ON \
	-DPLUGIN_HACKRF_SDR_SUPPORT=ON \
	-DPLUGIN_BLADERF_SDR_SUPPORT=ON \
	-DPLUGIN_MIRISDR_SDR_SUPPORT=ON \
	-DPLUGIN_RFNM_SDR_SUPPORT=ON \
	-DPLUGIN_RTLTCP_SUPPORT=ON \
	-DPLUGIN_SPYSERVER_SUPPORT=ON \
	-DPLUGIN_SDRPP_SERVER_SUPPORT=ON \
	-DPLUGIN_NET_SOURCE_SDR_SUPPORT=ON \
	-DPLUGIN_REMOTE_SDR_SUPPORT=ON \
	-DPLUGIN_AIRSPYHF_SDR_SUPPORT=ON \
	-DPLUGIN_LIMESDR_SDR_SUPPORT=ON \
	-DPLUGIN_PLUTOSDR_SDR_SUPPORT=ON \
	-DPLUGIN_USRP_SDR_SUPPORT=ON \
	-DPLUGIN_SDRPLAY_SDR_SUPPORT=OFF \
	-DPLUGIN_AARONIA_SDR_SUPPORT=OFF \
	-DPLUGIN_SOAPY_SDR_SUPPORT=OFF \
	-DPLUGIN_SDDC_SDR_SUPPORT=OFF \
	-DPLUGIN_RTAUDIO_SDR_SUPPORT=OFF \
	-DPLUGIN_PORTAUDIO_SINK=ON \
	-DPLUGIN_RTAUDIO_SINK=OFF \
	-DPLUGIN_AAUDIO_SINK=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_Armadillo=ON
ninja -C build
DESTDIR=$PKG ninja -C build install

for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	magick icon.png -resize ${s}x${s} \
		"$PKG/usr/share/icons/hicolor/${s}x${s}/apps/satdump.png"
done
cat > "$PKG/usr/share/applications/satdump.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=SatDump
GenericName=Satellite Decoder
Comment=Receive and decode weather and science satellites into images
Exec=satdump-ui
Terminal=false
Icon=satdump
Categories=Science;HamRadio;
Keywords=satellite;SDR;weather;NOAA;Meteor;LRPT;HRPT;APT;GOES;decoder;
StartupWMClass=satdump
DESKTOP
chmod 644 "$PKG/usr/share/applications/satdump.desktop"

test -x "$PKG/usr/bin/satdump-ui"
test -x "$PKG/usr/bin/satdump"
for p in rtlsdr_sdr_support hackrf_sdr_support airspyhf_sdr_support \
	limesdr_sdr_support plutosdr_sdr_support usrp_sdr_support \
	portaudio_audio_sink meteor_support dvb_support dvb_support_generic; do
	test -f "$PKG/usr/lib/satdump/plugins/lib$p.so"
done
