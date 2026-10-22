## About
[Qt WebEngine]{.idx} is Chromium's web engine wrapped as Qt 6 modules: QtWebEngineCore, the
`QWebEngineView` widget and the `WebEngineView` Qt Quick type, with the `QtWebEngineProcess`
helper that runs each page's renderer. The same build produces [Qt PDF]{.idx}, Chromium's PDF
renderer as the QtPdf and QtPdfWidgets libraries. Qt applications that show web content, help
pages or PDFs link it, and the Python bindings are
[python3-pyqt6-webengine](#port-python3-pyqt6-webengine).

The Chromium source is bundled inside the Qt release archive, under `src/3rdparty/chromium`, and
is built by its own GN and ninja run inside Qt's CMake build. Unlike every other `qt6-*` module,
this port follows Qt WebEngine's own release schedule and is built against whichever
[qt6-qtbase](#port-qt6-qtbase) the set is at (see
[How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built)).

## Flags
- `-D_LARGEFILE64_SOURCE` — in `CFLAGS` and `CXXFLAGS`: make musl declare `off64_t`, `stat64` and
  the other `*64` names, which it declares only under this macro.
- `-Wno-builtin-macro-redefined` — silence GCC's warning for a redefinition of a built-in macro
  such as `__DATE__` or `__TIME__`.
- `-Wno-deprecated-declarations` — silence the warning for every use of a declaration marked
  deprecated.
- `NINJAFLAGS="$MAKEFLAGS"` — the job limit of the Chromium ninja run inside Qt's build. It reads
  no other; unset, it starts a job per core, each taking 2 to 3 GB of memory.
- `NODEJS_EXECUTABLE=/usr/bin/node` — the Node.js the build runs its JavaScript tools with, among
  them the DevTools front end's build scripts. Chromium's `node.py` refuses to run without the
  variable, and Qt's ninja step also sets it, to the `node` its configure found;
  [nodejs](#port-nodejs) is in `depends` for it.
- `-DFEATURE_qtwebengine_build=ON` — with `-DFEATURE_qtpdf_build=ON`: build both Qt WebEngine and
  Qt PDF, each named so that a failed configure check stops the build instead of leaving a module
  out.
- `-DFEATURE_webengine_build_gn=ON` — build GN, Chromium's build-file generator, from Qt's copy in
  the archive. Qt accepts an installed GN only if its version is exactly the module's, which the
  tree's [gn](#port-gn), upstream's own, never reports.
- `-DFEATURE_webengine_rust_build=OFF` — leave out the parts of Chromium written in Rust, which
  need a Rust toolchain with bindgen and libclang. Upstream's default, named.
- `-DFEATURE_webengine_jumbo_build=8` — compile Chromium's sources in jumbo units, up to 8 files
  merged into each, which shortens the build. A number in place of `ON` sets the merge limit; 8 is
  the limit upstream assumes, and jumbo units are off by upstream's default.
- `-DFEATURE_webengine_developer_build=OFF` — with `-DFEATURE_webengine_full_debug_info=OFF`,
  `-DFEATURE_webengine_embedded_build=OFF` and `-DFEATURE_webenginedriver=OFF`: no developer
  configuration, no full debug information for Blink and V8, no embedded-device configuration and
  no WebEngineDriver, the WebDriver server for automated browsing.
- `-DFEATURE_webengine_ozone_x11=ON` — let the engine draw for a Qt program running on the X11
  platform plugin, under Xwayland. It needs libX11, libdrm, libXcomposite, libXcursor, libXrandr,
  libXi, xorgproto, libxshmfence, libXtst, libxkbcommon, libxkbfile and xcb-dri3. Upstream checks
  each one as optional, so without the switch a missing one would drop X11 support without an
  error, and a Qt program on X11 would have no web engine.
- `-DFEATURE_webengine_vulkan=ON` — with `-DFEATURE_webengine_vaapi=ON` and
  `-DFEATURE_webengine_system_gbm=ON`: Vulkan rendering, VA-API hardware video decoding through
  [libva](#port-libva), and buffer allocation through [mesa](#port-mesa)'s GBM.
- `-DFEATURE_webengine_system_alsa=ON` — with `-DFEATURE_webengine_system_pulseaudio=ON`: audio
  through ALSA and PulseAudio, which on this desktop reaches PipeWire.
- `-DFEATURE_webengine_webrtc=ON` — with `-DFEATURE_webengine_webrtc_pipewire=ON`: WebRTC, and
  screen sharing in it through PipeWire and the desktop portal, which upstream never turns on by
  itself.
- `-DFEATURE_webengine_proprietary_codecs=ON` — H.264, H.265 and MP3 in the engine's bundled
  FFmpeg. Off by upstream's default.
- `-DFEATURE_webengine_printing_and_pdf=ON` — with `pepper_plugins`, `spellchecker`, `extensions`,
  `geolocation`, `webchannel`, `kerberos` and `v8_context_snapshot`: printing and PDF output,
  Pepper plugins, the spellchecker, the extensions API, geolocation through
  [qt6-qtpositioning](#port-qt6-qtpositioning), Qt WebChannel, Kerberos authentication through
  [krb5](#port-krb5), and V8's start-up snapshot, each named.
- `-DFEATURE_webengine_system_icu=ON` — and the other `system_` switches set on: link the tree's
  ICU, GLib, zlib, minizip, libpng, libjpeg-turbo, libtiff, libwebp, OpenJPEG, Opus, Little CMS,
  HarfBuzz, FreeType, pciutils' libpci and libudev rather than Chromium's bundled copies.
- `-DFEATURE_webengine_system_ffmpeg=OFF` — Chromium's bundled FFmpeg: the [ffmpeg](#port-ffmpeg)
  port is a newer major version than Chromium's media code accepts.
- `-DFEATURE_webengine_system_libvpx=OFF` — Chromium's bundled libvpx: upstream builds VA-API only
  without a system libvpx.
- `-DFEATURE_webengine_system_libxml=OFF` — Chromium's bundled libxml2 and libxslt: Blink's XSLT
  code is written to an older libxml2 API than the [libxml2](#port-libxml2) port's.
- `-DFEATURE_webengine_system_re2=OFF` — with `-DFEATURE_webengine_system_snappy=OFF`,
  `-DFEATURE_webengine_system_openh264=OFF` and `-DFEATURE_webengine_webrtc_system_openh264=OFF`:
  Chromium's bundled re2, Snappy and OpenH264, though [re2](#port-re2), [snappy](#port-snappy)
  and [openh264](#port-openh264) are ports.

## Patches
- `0001-Enable-building-on-musl.patch` — removes the configure check that refuses to build Qt WebEngine on Linux without glibc.
- `0002-temp-failure-retry.patch` — defines glibc's `TEMP_FAILURE_RETRY` macro, which retries a call while it fails with `EINTR`, in the sandbox helper's `process_util.h`; musl does not provide it.
- `0003-qt-musl-mallinfo.patch` — compiles the calls to `mallinfo()` and `mallinfo2()` in Chromium's memory reporting only with glibc. musl's allocator has neither, and the malloc usage figure is reported as 0.
- `0004-qt-musl-resolve.patch` — compiles Chromium's resolver-reloading code (`dns_reloader.cc`) and its calls only with glibc, and makes `ScopedResState` clear and initialise the global `_res` state, as upstream does on OpenBSD, and free nothing when it is done; the patch notes that musl's `res_init()` allocates nothing.
- `0005-qtwebengine-missing-include.patch` — adds `<time.h>` to the includes of the Linux system-call header `linux_syscall_support.h`.
- `0006-no-execinfo.patch` — prints a stack trace only with glibc. musl has no `<execinfo.h>` and no `backtrace()`, so a trace is empty.
- `0007-musl-sandbox.patch` — adjusts the seccomp sandbox to musl's threads: a `clone()` is allowed when it carries the thread flags with any of `CLONE_SETTLS`, `CLONE_PARENT_SETTID`, `CLONE_CHILD_CLEARTID` and `CLONE_DETACHED`, rather than only glibc's exact set; it allows `set_tid_address`, `mremap` and `membarrier` outright, adds `pwritev2`, `sched_getparam`, `sched_getscheduler` and `sched_setscheduler` to the renderer's policy, and includes `<sys/syscall.h>` for the system-call numbers. Upstream's filter kills a process whose thread `clone()` carries any other flag set.
- `0008-missing-includes.patch` — adds `<inttypes.h>` to QUICHE's `window_manager.h` and `<sys/types.h>` to the sandbox's `credentials.h`, which use their types without including them.
- `gcc13.patch` — adds `<cstdint>` to `components/variations/seed_response.h`, which uses the fixed-width integer types without including their header.
- `gn-cstdint.patch` — the same for GN's `pool.h`: GN is built by its own `gen.py`, which takes no compiler flags from the environment, so the header has to name what it uses.
- `gn-ldflags.patch` — makes GN's `gen.py` start its link flags from the environment's `LDFLAGS`, so GN is linked with the tree's flags.
- `no-sandbox-settls.patch` — turns off the `CLONE_VM | CLONE_VFORK` shortcut in the sandbox's `credentials.cc`, which passes `CLONE_SETTLS` with a TLS pointer; musl's `clone()` answers any call carrying `CLONE_SETTLS` with `EINVAL`, and the code that makes the call aborts when it fails.
- `systypes.patch` — adds `<sys/types.h>` to the symboliser's `symbolize.h`.
- `fstatat-32bit.patch` — undefines `fstatat64` before `linux_syscall_support.h` declares its own `fstatat64` wrapper. musl's `<sys/stat.h>` defines `fstatat64` as a macro for `fstatat`, which would make the wrapper ask for a system call `__NR_fstatat` that does not exist. The block is compiled only where the kernel headers define `__NR_fstatat64`, which is not the case on x86-64.
- `devtools-frontend-compress_files.patch` — makes the DevTools front end's `compress_files.js` compress its files one after another rather than all at once, which keeps that step's memory use to one file at a time.
- `no-simd-fallback-fix.patch` — corrects the signature of WebAudio's scalar `Delay::ProcessARateVector()` fallback to match its declaration. The fallback is compiled only where neither x86 SIMD nor NEON is available, so this tree's x86-64 build never compiles it.
- `sandbox-path-max.patch` — adds `<limits.h>` to the sandbox's `proc_util.cc`, which sizes a buffer with `PATH_MAX`; on musl no header it includes declares it.

## Notes
After the Chromium build, `build.sh` checks that no object was compiled under
`third_party/zlib/bundled_zlib` or `third_party/zlib/bundled_minizip`: Chromium compiles its own
zlib and minizip whenever the system ones are not taken, and the check fails the build rather than
ship a second copy of each. [minizip](#port-minizip) is in `depends` for that reason.

`QtWebEngineProcess` is installed in `/usr/lib/qt6/libexec`, where the engine starts it from.
`build.sh` removes the `/usr/bin/QtWebEngineProcess6` that the install also puts on `PATH`;
upstream describes the helper as Qt WebEngine's internal process.

Of the engine's translated locale packs, only `en-US.pak` and `en-GB.pak` ship. The engine falls
back to `en-US.pak` for any locale whose pack is missing.

The renderer sandbox is built on unprivileged user namespaces, so no setuid helper is installed
(see [setuid binaries](../../../../docs/kdos/03-architecture/security-model.md#setuid-binaries)).
