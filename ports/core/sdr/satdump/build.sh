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

# GLFW gives a window no Wayland app_id and an X11 class of its title unless
# asked; the patch names both satdump, which the entry's StartupWMClass names.
patch -p1 -i "$PORT_SRC/satdump-app-id.patch"

# The DVB plugin compiles its decoders with -msse4.1 and, on a CPU without
# SSE4.1, declines to load, so DVB-S and DVB-S2 vanish on a baseline x86-64
# machine. There is no flag short of dropping SSE4.1 for the simd_sse41
# plugin as well: FindSSE41's one cache variable serves both. The patch adds a
# second build of the same sources without the flag, libdvb_support_generic,
# which registers the modules only on a CPU the SSE4.1 build refuses. Plugins
# are opened RTLD_LOCAL, so the two copies' symbols never bind to each other.
patch -p1 -i "$PORT_SRC/satdump-dvb-generic.patch"

# calibration.h declares a function taking time_t without including <ctime>,
# and neither <map> nor <math.h> is required to declare it.
patch -p1 -i "$PORT_SRC/satdump-ctime.patch"

# The bundled sol2 has a template member that names a member its class does
# not have; GCC 15 checks template bodies nobody instantiates and rejects it
# unless -Wtemplate-body is off.
export CXXFLAGS="$CXXFLAGS -Wno-template-body"

# Every decoder plugin is built (PLUGINS_ALL). Radios: RTL-SDR, Airspy,
# HackRF, bladeRF, MiriSDR and RFNM (their drivers are built in over libusb),
# and the network sources: rtl_tcp, SpyServer, SDR++ server, a raw network
# stream and SatDump's own remote. Airspy HF+, LimeSDR, PlutoSDR, USRP,
# SDRplay and Aaronia need libraries that are not ports; the SoapySDR source
# is upstream's "not recommended" path and SDDC is experimental. Audio leaves
# through PortAudio, which PipeWire answers; the RtAudio and AAudio sinks are
# off. OpenCL accelerates what it can through ocl-icd and falls back to the
# CPU without an ICD. Armadillo is never looked up.
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
	-DPLUGIN_AIRSPYHF_SDR_SUPPORT=OFF \
	-DPLUGIN_LIMESDR_SDR_SUPPORT=OFF \
	-DPLUGIN_PLUTOSDR_SDR_SUPPORT=OFF \
	-DPLUGIN_USRP_SDR_SUPPORT=OFF \
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

# Upstream's entry names its icon by absolute path and its window class by the
# title, which carries the version. The 2234-pixel logo is scaled to the
# sizes the panel reads.
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
for p in rtlsdr_sdr_support hackrf_sdr_support portaudio_audio_sink meteor_support \
	dvb_support dvb_support_generic; do
	test -f "$PKG/usr/lib/satdump/plugins/lib$p.so"
done
