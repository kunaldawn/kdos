## About

LibreWolf is a fork of Firefox with telemetry removed and its privacy settings on by default. It
is built from LibreWolf's source tarball, which is Firefox's source with LibreWolf's patches,
branding and settings already applied, and follows Firefox's release line rather than the ESR one
[firefox-esr](#port-firefox-esr) follows. The recipe is firefox-esr's with LibreWolf's names and
LibreWolf's own build options.

## Flags

The switches this build shares with firefox-esr (the toolchain, the Rust target and wrapper, the
run path, the mach variables, the feature switches and the system libraries) do the same thing
here and are explained on [firefox-esr's page](#port-firefox-esr). These are the ones that
differ.

- `-ftrivial-auto-var-init=zero`, `-fwrapv` — Added to the front of `CFLAGS` and `CXXFLAGS`.
  The first has clang fill every automatic variable that is not initialised with zeros; the second
  makes signed integer overflow wrap instead of leaving it undefined, so the optimiser cannot
  assume it never happens. Both come from the `mozconfig` LibreWolf ships in its tarball.
- `-Wl,-rpath,/usr/lib/librewolf` — The run path into the program's private directory, for the
  same musl reason as firefox-esr's.
- `--with-app-name=librewolf`, `MOZ_APP_REMOTINGNAME=librewolf` — The program's name and its
  internal name, which is the Wayland app id. LibreWolf's own `mozconfig` sets the remoting name
  to `LibreWolf`; here it is the same lower-case name as the binary, `/usr/lib/librewolf`, the
  desktop entry and `StartupWMClass`, so the panel finds the entry for the window.
- `--with-branding=browser/branding/librewolf` — LibreWolf's name and artwork.
- `MOZ_REQUIRE_SIGNING=` — Empty, so the build does not enforce that add-ons are signed by
  the trusted root. LibreWolf's `mozconfig` sets it the same way.
- `--enable-stl-hardening` — Turns on the C++ standard library's hardening checks, as
  LibreWolf's `mozconfig` does.
- `MOZ_CRASHREPORTER=0`, `MOZ_DATA_REPORTING=0`, `MOZ_SERVICES_HEALTHREPORT=0`,
  `MOZ_TELEMETRY_REPORTING=0` — Set for make through `mk_add_options`, as in LibreWolf's
  `mozconfig`: no crash reporter, data reporting, health report or telemetry.

## Patches

- `fix-rust-target.patch` — In `build/moz.configure/rust.configure`, takes the rustc target from the `RUST_TARGET` environment variable instead of matching the C target (`x86_64-pc-linux-musl`) against rustc's list of targets.
- `lfs64.patch` — Makes three places use `stat`, `lstat` and `fstat` rather than their `*64` forms: `xpcom/io/nsLocalFileUnix.h`, the base profiler's `mozglue/baseprofiler/core/shared-libraries-linux.cc`, and the sandbox broker's `SandboxBrokerUtils.h`. musl's `stat` is already 64-bit, and its headers spell the `*64` names only as macros, under `_LARGEFILE64_SOURCE`.
- `musl-no-linux-prctl.patch` — Drops `#include <linux/prctl.h>` from libwebrtc's `rtc_base/platform_thread_types.cc`. musl's `<sys/prctl.h>` defines `struct prctl_mm_map` as well, and with both headers the compile fails on the redefinition.
- `musl-pthread-t.patch` — In the vendored `audio_thread_priority` crate, serialises and deserialises the thread's `pthread_t` through `usize`, and updates the crate's `.cargo-checksum.json` to match. musl's `pthread_t` is a pointer, which has no `to_ne_bytes` or `from_ne_bytes`; glibc's is an integer.
- `rust-lto-thin.patch` — In `config/makefiles/rust.mk`, links the Rust crates with thin LTO, `-Clto=thin`, instead of `-Clto`.
- `sandbox-sched_setscheduler.patch` — In the seccomp filters of the media plugin (GMP) and remote data decoder (RDD) processes, allows `sched_setscheduler` outright. Upstream allows it for the calling thread and returns `EPERM` for any other thread id; the patch names Mozilla bug 1657849.

## Notes

The configuration is written to `$SRC/kdos.mozconfig` and named by `MOZCONFIG`, so the `mozconfig`
LibreWolf ships at the top of its tarball is left as it is and not read. That file's other
choices are therefore not in this build: it uses jemalloc with a replaceable allocator (this
build passes `--disable-jemalloc`), enables Rust SIMD and proxy bypass protection, disables the
default-browser agent at build time, sets the `release` update channel and `MOZILLA_OFFICIAL=1`,
and points `--with-l10n-base` at its bundled translations.

The policies file adds to firefox-esr's: no default bookmarks, HTTPS-only mode, no Encrypted Media
Extensions, content analysis off at every interception point, local network access protection on,
with trackers blocked and a prompt, an update URL of `https://localhost`, no default-browser agent,
language packs refused as extensions, and the new-tab top sites and highlights off. `kdos.js` keeps only the spelling dictionary path and the
portal file chooser, and no `distribution.ini` is written.
