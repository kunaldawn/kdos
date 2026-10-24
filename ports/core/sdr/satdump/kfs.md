## About

[SatDump]{.idx} receives, demodulates and decodes the downlinks of weather and science satellites
(NOAA and MetOp HRPT and APT, Meteor LRPT, GOES, FengYun, and many more) and turns them into
images and calibrated products. It runs live from a radio or from a recording: `satdump-ui` is the
OpenGL interface, drawn through GLFW and Dear ImGui, and `satdump` the command-line pipeline runner.
Each satellite family, radio and audio sink is a plugin under `/usr/lib/satdump/plugins`.

## Flags

- `CXXFLAGS="$CXXFLAGS -Wno-template-body -DSOL_NO_LUA_HPP=1"` — two fixes for the bundled sol2,
  the C++ binding of the Lua that SatDump compiles in. sol2 has a template member that names a
  member its class does not have; GCC checks template bodies that are never instantiated and
  rejects it unless `-Wtemplate-body` is off. And SatDump puts the headers of its bundled Lua
  first on the include path, but sol2 includes `<lua.hpp>`, which the bundle lacks, so the
  system [lua](#port-lua)'s copy is found and pulls in the system release's `lua.h`, which sol2
  rejects. `SOL_NO_LUA_HPP` makes sol2 include `lua.h`, `lualib.h` and `lauxlib.h` itself, all
  three from the bundle.
- `-DBUILD_GUI=ON`, `-DBUILD_GLES=OFF` — Build `satdump-ui`, on desktop OpenGL rather than OpenGL
  ES.
- `-DBUILD_TOOLS=OFF` — No developer tool programs; upstream's default.
- `-DBUILD_ZIQ=ON`, `-DBUILD_ZIQ2=OFF` — Read and write SatDump's compressed ZIQ baseband format,
  with [zstd](#port-zstd); not the work-in-progress ZIQ2.
- `-DBUILD_OPENCL=ON`, `-DBUILD_OPENMP=ON` — GPU acceleration through OpenCL, by the
  [ocl-icd](#port-ocl-icd) loader, which falls back to the CPU when no OpenCL driver is installed,
  and OpenMP on the CPU.
- `-DBUILD_DOCS=OFF` — No Doxygen documentation.
- `-DENABLE_INSTALL=ON` — Generate the install target; upstream's default.
- `-DPLUGINS_ALL=ON` — Build every satellite decoder plugin.
- `-DPLUGIN_SCRIPTING=OFF`, `-DPLUGIN_BOCHUM_SUPPORT=OFF`, `-DPLUGIN_BITVIEW_APP=OFF`,
  `-DPLUGIN_GVAR_EXTENDED=OFF` — Leave out the experimental scripting plugin, the Bochum-specific plugin, the work-in-progress BitView application and the GVAR
  Extended plugin, none of which `PLUGINS_ALL` covers.
- `-DPLUGIN_RTLSDR_SDR_SUPPORT=ON` — and the other radio sources set `ON`: RTL-SDR, Airspy,
  HackRF, bladeRF, MiriSDR and RFNM, Airspy HF+, LimeSDR, PlutoSDR (through
  [libiio](#port-libiio) and [libad9361](#port-libad9361)) and USRP (through [uhd](#port-uhd)), and
  the network sources: rtl_tcp, SpyServer, the SDR++ server, a raw network stream, and SatDump's
  own remote source.
- `-DPLUGIN_SDRPLAY_SDR_SUPPORT=OFF`, `-DPLUGIN_AARONIA_SDR_SUPPORT=OFF` — SDRplay and Aaronia
  radios need vendor libraries that are not ports.
- `-DPLUGIN_SOAPY_SDR_SUPPORT=OFF`, `-DPLUGIN_SDDC_SDR_SUPPORT=OFF`,
  `-DPLUGIN_RTAUDIO_SDR_SUPPORT=OFF` — No SoapySDR source, which upstream marks "not recommended",
  no SDDC (RX666, RX888) source, and no RtAudio input source; all three are upstream's defaults.
- `-DPLUGIN_PORTAUDIO_SINK=ON`, `-DPLUGIN_RTAUDIO_SINK=OFF`, `-DPLUGIN_AAUDIO_SINK=OFF` — Audio
  leaves through [portaudio](#port-portaudio), which PipeWire answers; AAudio is Android's audio
  interface.
- `-DCMAKE_DISABLE_FIND_PACKAGE_Armadillo=ON` — Never look up Armadillo, which `src-core` uses when
  it finds it. Armadillo is not a port.

## Patches

- `satdump-app-id.patch` — sets GLFW's `GLFW_WAYLAND_APP_ID` and `GLFW_X11_CLASS_NAME` hints to `satdump` before both window creations in `src-ui/main.cpp`, the first and the OpenGL 2.1 fallback. Unless asked, GLFW gives a window no Wayland app id and an X11 class of its title, which carries the version; the desktop entry's `StartupWMClass` names `satdump`.
- `satdump-dvb-generic.patch` — the DVB plugin compiles its decoders with `-msse4.1` and, on a CPU without SSE4.1, declines to load, so DVB-S and DVB-S2 would be missing on a baseline x86-64 machine. No flag avoids that short of dropping SSE4.1 for the `simd_sse41` plugin as well, since the one cache variable of `FindSSE41` serves both. The patch adds a second build of the same sources without the flag, `libdvb_support_generic`, which registers the DVB modules only on a CPU the SSE4.1 build refuses.
- `satdump-ctime.patch` — includes `<ctime>` in `src-core/common/calibration.h`, which declares a function taking a `time_t`; neither `<map>` nor `<math.h>` is required to declare it.
- `satdump-calibration-constants.patch` — turns the macros `c1`, `c2` and `e_num` of `calibration.h` into `constexpr double` constants. `volk_common.h` of [libvolk](#port-libvolk), included after it, names its own locals `c1` and `c2`, and the macros turn those declarations into numbers; a local may shadow a constant.

## Notes

SatDump opens its plugins with `dlopen` and no `RTLD_GLOBAL`, so the two DVB builds' symbols never
bind to each other.

The drivers of RTL-SDR, Airspy, HackRF, bladeRF, Airspy HF+ and LimeSDR come from their ports; the
MiriSDR and RFNM sources carry their own drivers over [libusb](#port-libusb). A radio source's
plugin is built only when `find_library` finds its library, and otherwise skipped with only a
message, so a switch set `ON` does not by itself guarantee the plugin; the check at the end of
`build.sh` does.

Upstream's desktop entry names its icon by absolute path and its window class by the title.
`build.sh` scales the large `icon.png` logo to 48, 64, 128 and 256 pixels with ImageMagick and
writes an entry of its own. It ends by checking that both programs and a list of plugins (the
RTL-SDR, HackRF, Airspy HF+, LimeSDR, PlutoSDR and USRP sources, the PortAudio sink, the Meteor
decoder and both DVB builds) are in the package.
