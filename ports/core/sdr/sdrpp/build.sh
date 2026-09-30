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

# The release line stopped at 1.0.4 and upstream publishes only rolling
# builds, so the port is pinned to a master commit.
#
# Sources: Airspy, Airspy HF+, bladeRF, HackRF, LimeSDR, PlutoSDR (libiio
# with libad9361), RFNM, Perseus, RTL-SDR, USRP (UHD) and SoapySDR by
# library, plus the driverless network ones (rtl_tcp, SpyServer, SDR++
# server, Hermes, RFspace, Spectran HTTP, a raw network stream), a file and a
# sound card. SDRplay and the vendor-SDK radios need libraries that are not
# ports. Audio goes in and out through RtAudio, which PipeWire answers. The
# Open and Save buttons run zenity for their file chooser and do nothing
# without it. The Discord presence module is off; the scheduler,
# DAB, RyFi, VOR and weather-satellite modules are upstream's experiments and
# stay off with it. libcorrect is the copy upstream carries; HAVE_SSE=OFF
# skips its -march=native probe, which would build it for SSE 4.1.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DOPT_BACKEND_GLFW=ON \
	-DOPT_BACKEND_ANDROID=OFF \
	-DOPT_OVERRIDE_STD_FILESYSTEM=OFF \
	-DUSE_INTERNAL_LIBCORRECT=ON \
	-DHAVE_SSE=OFF \
	-DUSE_BUNDLE_DEFAULTS=OFF \
	-DOPT_BUILD_AIRSPY_SOURCE=ON \
	-DOPT_BUILD_AIRSPYHF_SOURCE=ON \
	-DOPT_BUILD_AUDIO_SOURCE=ON \
	-DOPT_BUILD_BADGESDR_SOURCE=OFF \
	-DOPT_BUILD_BLADERF_SOURCE=ON \
	-DOPT_BUILD_DRAGONLABS_SOURCE=OFF \
	-DOPT_BUILD_FILE_SOURCE=ON \
	-DOPT_BUILD_FOBOSSDR_SOURCE=OFF \
	-DOPT_BUILD_HACKRF_SOURCE=ON \
	-DOPT_BUILD_HAROGIC_SOURCE=OFF \
	-DOPT_BUILD_HERMES_SOURCE=ON \
	-DOPT_BUILD_HYDRASDR_SOURCE=OFF \
	-DOPT_BUILD_KCSDR_SOURCE=OFF \
	-DOPT_BUILD_LIMESDR_SOURCE=ON \
	-DOPT_BUILD_NETWORK_SOURCE=ON \
	-DOPT_BUILD_PERSEUS_SOURCE=ON \
	-DOPT_BUILD_PLUTOSDR_SOURCE=ON \
	-DOPT_BUILD_RFNM_SOURCE=ON \
	-DOPT_BUILD_RFSPACE_SOURCE=ON \
	-DOPT_BUILD_RTL_SDR_SOURCE=ON \
	-DOPT_BUILD_RTL_TCP_SOURCE=ON \
	-DOPT_BUILD_SDRPP_SERVER_SOURCE=ON \
	-DOPT_BUILD_SDRPLAY_SOURCE=OFF \
	-DOPT_BUILD_SOAPY_SOURCE=ON \
	-DOPT_BUILD_SPECTRAN_SOURCE=OFF \
	-DOPT_BUILD_SPECTRAN_HTTP_SOURCE=ON \
	-DOPT_BUILD_SPYSERVER_SOURCE=ON \
	-DOPT_BUILD_USRP_SOURCE=ON \
	-DOPT_BUILD_ANDROID_AUDIO_SINK=OFF \
	-DOPT_BUILD_AUDIO_SINK=ON \
	-DOPT_BUILD_NETWORK_SINK=ON \
	-DOPT_BUILD_NEW_PORTAUDIO_SINK=OFF \
	-DOPT_BUILD_PORTAUDIO_SINK=OFF \
	-DOPT_BUILD_ATV_DECODER=ON \
	-DOPT_BUILD_DAB_DECODER=OFF \
	-DOPT_BUILD_FALCON9_DECODER=OFF \
	-DOPT_BUILD_KG_SSTV_DECODER=OFF \
	-DOPT_BUILD_M17_DECODER=ON \
	-DOPT_BUILD_METEOR_DEMODULATOR=ON \
	-DOPT_BUILD_PAGER_DECODER=ON \
	-DOPT_BUILD_RADIO=ON \
	-DOPT_BUILD_RYFI_DECODER=OFF \
	-DOPT_BUILD_VOR_RECEIVER=OFF \
	-DOPT_BUILD_WEATHER_SAT_DECODER=OFF \
	-DOPT_BUILD_DISCORD_PRESENCE=OFF \
	-DOPT_BUILD_FREQUENCY_MANAGER=ON \
	-DOPT_BUILD_IQ_EXPORTER=ON \
	-DOPT_BUILD_RECORDER=ON \
	-DOPT_BUILD_RIGCTL_CLIENT=ON \
	-DOPT_BUILD_RIGCTL_SERVER=ON \
	-DOPT_BUILD_SCANNER=ON \
	-DOPT_BUILD_SCHEDULER=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

# Upstream's entry names its icon by absolute path, which the panel does not
# resolve. The GLFW backend sets the Wayland app_id to sdrpp.
install -Dm644 root/res/icons/sdrpp.png \
	"$PKG/usr/share/icons/hicolor/512x512/apps/sdrpp.png"
cat > "$PKG/usr/share/applications/sdrpp.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=SDR++
GenericName=Software Defined Radio
Comment=Receive AM, FM and SSB with a spectrum and waterfall display
Exec=sdrpp
Terminal=false
Icon=sdrpp
Categories=HamRadio;AudioVideo;
Keywords=SDR;radio;receiver;waterfall;rtl-sdr;airspy;hackrf;scanner;
StartupWMClass=sdrpp
DESKTOP
chmod 644 "$PKG/usr/share/applications/sdrpp.desktop"

test -x "$PKG/usr/bin/sdrpp"
test -f "$PKG/usr/lib/sdrpp/plugins/rtl_sdr_source.so"
