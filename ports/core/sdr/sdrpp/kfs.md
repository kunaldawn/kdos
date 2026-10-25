## About

SDR++ is a software-defined radio receiver with a spectrum, a waterfall and demodulators for
AM, FM, SSB, CW and more. It draws its whole interface itself with Dear ImGui on OpenGL through
[glfw](#port-glfw), so it links no toolkit. Its radio sources, audio sinks, decoders and helpers
are separate plugin modules, installed under `/usr/lib/sdrpp/plugins` and chosen at build time one
by one.

## Flags

- `-DOPT_BACKEND_GLFW=ON` — the desktop back end: a GLFW window with OpenGL, rather than the Android
  back end (`-DOPT_BACKEND_ANDROID=OFF`).
- `-DOPT_OVERRIDE_STD_FILESYSTEM=OFF` — use the C++ library's own `std::filesystem` rather than the
  replacement under `std_replacement/`, which upstream carries for systems that lack one.
- `-DUSE_INTERNAL_LIBCORRECT=ON` — build the bundled copy of libcorrect (forward error
  correction, used by the digital decoders) and link it statically into the core. libcorrect is
  not a port.
- `-DHAVE_SSE=OFF` — the bundled libcorrect's configure compiles a probe with `-march=native` to
  decide whether to add `-msse4.1` and its SSE code; presetting the result to `OFF` skips the probe,
  so what is built does not depend on the CPU of the machine that builds it.
- `-DUSE_BUNDLE_DEFAULTS=OFF` — keep the default resource and module directories of an ordinary
  install under `/usr`; on, they point inside a macOS `.app` bundle.
- `-DOPT_BUILD_AIRSPY_SOURCE=ON` — and every other `OPT_BUILD_*_SOURCE=ON`: one source module per
  radio or network protocol. The hardware ones link [airspy](#port-airspy),
  [airspyhf](#port-airspyhf), [bladerf](#port-bladerf), [hackrf](#port-hackrf),
  [limesuite](#port-limesuite), [libperseus-sdr](#port-libperseus-sdr), [libiio](#port-libiio) with
  [libad9361](#port-libad9361) for the PlutoSDR, [librfnm](#port-librfnm), [rtl-sdr](#port-rtl-sdr),
  [soapysdr](#port-soapysdr) and [uhd](#port-uhd); the audio source uses [rtaudio](#port-rtaudio).
  The file, Hermes, network, RFspace, RTL-TCP, SDR++ server, Spectran HTTP and SpyServer sources
  need no library. Upstream's defaults leave the bladeRF, LimeSDR, Perseus, RFNM, SoapySDR and
  USRP modules off.
- `-DOPT_BUILD_DRAGONLABS_SOURCE=OFF` — and the Harogic, HydraSDR, KCSDR, SDRplay and Spectran
  sources: each needs a vendor library (`libdlcr`, `htra_api`, `libhydrasdr`, `libkcsdr`,
  SDRplay's API, Aaronia's RTSA suite) that is not a port.
- `-DOPT_BUILD_BADGESDR_SOURCE=OFF` — the top-level CMakeLists names a `badgesdr_source` directory
  that this source tree does not contain, so turning it on stops the configure.
- `-DOPT_BUILD_FOBOSSDR_SOURCE=OFF` — the RigExpert Fobos module, off; it would link
  [libfobos](#port-libfobos), which is not in this port's `depends`.
- `-DOPT_BUILD_AUDIO_SINK=ON` — sound goes out through rtaudio; with `-DOPT_BUILD_NETWORK_SINK=ON`
  it can also be streamed over the network. The two PortAudio sinks and the Android one are off.
- `-DOPT_BUILD_M17_DECODER=ON` — the M17 digital voice decoder, which decodes speech with
  [codec2](#port-codec2); upstream leaves it off by default.
- `-DOPT_BUILD_DAB_DECODER=OFF` — and the Falcon 9, KG-SSTV, RyFi, VOR and weather-satellite
  decoders: upstream's experimental modules, off as upstream's defaults have them. The Falcon 9
  decoder would also run `ffplay`.
- `-DOPT_BUILD_DISCORD_PRESENCE=OFF` — no Discord Rich Presence module, which upstream builds by
  default and which reports what the receiver is tuned to into a running Discord client.
- `-DOPT_BUILD_SCHEDULER=OFF` — no scheduler module, another of upstream's experiments and off by
  default.

## Notes

Upstream releases SDR++ on a rolling model, as a nightly build, so the recipe follows a commit on
the main branch, named by `_commit` in `kpkgbuild`.

The file pickers and the frequency manager's import and export use the bundled
portable-file-dialogs, which draws no window of its own: it runs `zenity`, `kdialog`, `matedialog`
or `qarma`, whichever it finds. [zenity](#port-zenity) is in `depends` for that. The GLFW back end
sets the Wayland app id to `sdrpp`, which the recipe's desktop entry repeats in `StartupWMClass`.
The recipe writes that entry over upstream's, which names its icon by an absolute path under
`/usr/share/sdrpp/icons`, and installs the PNG icon into the hicolor theme under the name `sdrpp`.
`build.sh` ends by checking that the RTL-SDR source plugin was installed.
