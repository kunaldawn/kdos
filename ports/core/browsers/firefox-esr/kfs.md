## About

Firefox ESR is Mozilla's Extended Support Release of Firefox, and it is KDOS's default web
browser: web pages and the `http` and `https` schemes open in it through
`/etc/xdg/kdos-mimeapps.list`. It is built from Mozilla's source tarball with clang, lld and Rust,
on GTK 3 with both its Wayland and X11 backends; the session's `MOZ_ENABLE_WAYLAND=1` puts it on Wayland (see
[toolkits, Wayland and X11](../../../../docs/kdos/02-user-guide/applications.md#toolkits-wayland-and-x11)).
[librewolf](#port-librewolf) is built from the same recipe shape.

## Flags

- `CC=clang`, `CXX=clang++` — Mozilla builds with clang; the C++ parts of the Rust crates are
  compiled by clang as well.
- `--gcc-triple=$(gcc -dumpmachine)` — Added to `CFLAGS`, `CXXFLAGS` and `LDFLAGS`. The `cc`
  crate passes clang Rust's own target, `x86_64-unknown-linux-musl`, and clang then looks for a
  gcc installation under that name, finds none, and loses the C++ headers; naming gcc's triple
  points the search at gcc's own installation (see
  [an LLVM that guessed its triple](../../../../docs/kdos/05-developer/build-troubleshooting.md#an-llvm-that-guessed-its-triple)).
- `${CFLAGS/-std=gnu11/}` — Removes the tree's `-std=gnu11` from `CFLAGS`, so each C source is
  compiled with the standard Mozilla's build gives it.
- `-Wl,-rpath,/usr/lib/firefox-esr` — A run path into the program's private directory. The
  launcher loads `libxul.so` and its neighbours there by full path, and musl, unlike glibc, does
  not take an already-loaded library for a bare-name `DT_NEEDED`, so without it libxul fails to
  load (see
  [a private library loaded by full path, then not found](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-private-library-loaded-by-full-path-then-not-found)).
- `-C target-feature=-crt-static`, `RUSTC_WRAPPER` — Rust's musl target links statically by
  default, and a statically linked build script cannot load libclang for bindgen. `RUSTFLAGS`
  turns static linking off, but mach always passes `--target`, and cargo then gives `RUSTFLAGS`
  to the target's crates alone; the `rustc-dynamic` wrapper appends the same flag to every rustc
  call, build scripts included (see
  [Rust with a binding generator](../../../../docs/kdos/05-developer/build-troubleshooting.md#rust-with-a-binding-generator)).
- `RUST_TARGET=x86_64-unknown-linux-musl` — The Rust target, read by `fix-rust-target.patch`.
- `MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE=none` — Mach installs none of its Python
  dependencies into a virtual environment with pip, which would need the network, and checks
  none against the system; it runs on the packages vendored in the source tree.
- `MOZBUILD_STATE_PATH` — Mach's state directory, `~/.mozbuild` by default, kept inside the
  source tree instead.
- `MOZ_NOSPAM=1` — No desktop notification when the build finishes.
- `MOZ_BUILD_DATE` — The 14-digit build ID compiled into the program. It is taken from
  `SOURCE_DATE_EPOCH`, so two builds of the same recipe carry the same ID; without it Mozilla
  uses the time of the build.
- `--with-app-name=firefox-esr`, `MOZ_APP_REMOTINGNAME=firefox-esr` — The program's name, and its
  internal name, which is the Wayland app id and the profile and remoting name. With an update
  channel other than `release` the remoting name would default to `firefox-esr-esr`. The binary,
  `/usr/lib/firefox-esr`, the desktop entry, the app id and `StartupWMClass` all carry the one
  name, so the panel finds the entry for the window.
- `--enable-official-branding` — Build with the Firefox name and logo from
  `browser/branding/official` rather than the unbranded artwork. Mozilla's help text for the switch
  asks that such builds be distributed only with permission under its trademark policy.
- `--enable-update-channel=esr` — Build for the ESR channel rather than `default`.
- `--with-distribution-id=org.kdos` — The distribution id, `org.mozilla` by default.
- `MOZ_TELEMETRY_REPORTING=` — Empty, so telemetry reporting is compiled out.
- `--enable-application=browser`, `--enable-default-toolkit=cairo-gtk3-x11-wayland` — Build the
  browser on GTK 3 with both the Wayland and X11 backends.
- `--enable-release`, `--enable-optimize=-O2`, `--enable-hardening` — Release configuration,
  compiled at `-O2`, with Mozilla's hardening flags. Release is the default for a release or beta
  milestone, which an ESR is, and hardening is on unless it is disabled; both are named so neither
  rests on a default.
- `--enable-linker=lld` — Link with LLVM's lld.
- `--with-libclang-path=/usr/lib` — Where bindgen finds `libclang.so`.
- `--without-wasm-sandboxed-libraries` — Compile in none of the libraries Mozilla runs inside a
  WebAssembly sandbox (graphite, ogg, hunspell, expat, woff2 and soundtouch, by default). That
  build needs a WASI sysroot and a compiler for the wasm32-wasi target, which no port provides; the libraries are linked as
  ordinary code instead.
- `--disable-bootstrap` — Mach downloads no toolchains of its own.
- `--disable-cargo-incremental` — No incremental Rust compilation.
- `--disable-crashreporter`, `--disable-updater` — No crash reporter and no updater; the package
  manager updates the program.
- `--disable-eme` — No Encrypted Media Extensions, the hook through which Firefox downloads and
  runs the closed Widevine module for protected video.
- `--disable-jemalloc` — Use the C library's `malloc` rather than Mozilla's jemalloc, which is
  the default on Linux.
- `--disable-necko-wifi` — No Wi-Fi scanner in the network library.
- `--disable-debug`, `--disable-debug-symbols`, `--disable-tests` — No debug build, no debugging
  information, and none of the test libraries and programs.
- `--disable-strip`, `--disable-install-strip` — Mozilla does not strip anything itself; kpkg's
  own symbol policy applies to the package (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `--enable-dbus`, `--enable-alsa`, `--enable-pulseaudio` — D-Bus, and sound through ALSA and
  through the PulseAudio API, which `pipewire-pulse` serves (see
  [audio](../../../../docs/kdos/03-architecture/session.md#audio)).
- `--disable-jack`, `--disable-sndio` — No JACK or sndio audio backend, so sound goes only
  through the PulseAudio and ALSA backends above. sndio is not a port; the JACK library is
  [pipewire](#port-pipewire)'s `pipewire-jack`, which Firefox is then not built against.
- `--with-system-nspr`, `--with-system-nss`, `--with-system-icu`, `--with-system-ffi`,
  `--with-system-libevent`, `--with-system-jpeg`, `--with-system-libvpx`, `--with-system-webp`,
  `--with-system-zlib`, `--with-system-pixman`, `--with-system-pipewire`, `--with-system-gbm`,
  `--with-system-libdrm` — Link the tree's own ports of these libraries instead of compiling the
  copies in Mozilla's source.
- `MOZ_OBJDIR=$SRC/obj` — The object directory, inside the source tree.

## Patches

- `fix-rust-target.patch` — In `build/moz.configure/rust.configure`, takes the rustc target from the `RUST_TARGET` environment variable instead of matching the C target (`x86_64-pc-linux-musl`) against rustc's list of targets, and imports what that needs.
- `lfs64.patch` — Makes three places use `stat`, `lstat` and `fstat` rather than their `*64` forms: `xpcom/io/nsLocalFileUnix.h`, the base profiler's `mozglue/baseprofiler/core/shared-libraries-linux.cc`, and the sandbox broker's `SandboxBrokerUtils.h`. musl's `stat` is already 64-bit, and its headers spell the `*64` names only as macros, under `_LARGEFILE64_SOURCE`.
- `musl-no-linux-prctl.patch` — Drops `#include <linux/prctl.h>` from libwebrtc's `rtc_base/platform_thread_types.cc`. musl's `<sys/prctl.h>` defines `struct prctl_mm_map` as well, and with both headers the compile fails on the redefinition.
- `rust-lto-thin.patch` — In `config/makefiles/rust.mk`, links the Rust crates with thin LTO (`-Clto=thin`) instead of the default `-Clto`, and, on riscv64, with no LTO at all.
- `sandbox-sched_setscheduler.patch` — In the seccomp filters of the media plugin (GMP) and remote data decoder (RDD) processes, allows `sched_setscheduler` outright. Upstream allows it for the calling thread and returns `EPERM` for any other thread id; the patch names Mozilla bug 1657849.

## Notes

After `mach install`, `build.sh` adds what makes the package KDOS's:

- `distribution/policies.json`, Firefox's enterprise policies, which turn off telemetry,
  studies, update checks, Firefox accounts, the default-browser check, sponsored and suggested
  content and the AI features, and lock DNS over HTTPS off;
- `distribution/distribution.ini`, naming the distributor;
- `browser/defaults/preferences/kdos.js`, which points the spelling checker at the
  [hunspell-en](#port-hunspell-en) dictionaries in `/usr/share/hunspell`, opens files through the
  desktop's FileChooser portal, and turns off the media plugin downloads (OpenH264) and the
  remaining data reporting;
- the branding PNGs installed as `firefox-esr` icons in hicolor, and `firefox-esr.desktop`, named
  plain `Firefox`.

When `mach install` leaves `firefox-esr-bin` as a second copy of the program, it is replaced with
a link to `firefox-esr`.

The recipe's `series` key keeps `ports/update` offering only releases on the ESR line it names.
