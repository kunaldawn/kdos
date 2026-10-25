## About
[Thunderbird]{.idx} is Mozilla's mail, calendar, address-book, chat and feed client, built from
its extended-support release on the same Gecko engine as [firefox-esr](#port-firefox-esr). It is
the graphical mail client of KDOS and the default handler for `mailto:` links, `message/rfc822`
and `text/calendar` in `/etc/xdg/kdos-mimeapps.list`. Everything it does works offline once an
account is set up by hand. The paths that reach the network on their own (update checks,
telemetry, add-on updates, the start page, captive-portal and connectivity probes, safe-browsing
lists) are off, by build switch where one exists and by default prefs and policies where none does.

## Flags
- `CC=clang` — with `CXX=clang++`, `AR=llvm-ar`, `NM=llvm-nm`, `RANLIB=llvm-ranlib` and
  `--enable-linker=lld`: Gecko is compiled with [clang](#port-clang) and linked with
  [lld](#port-lld) rather than the phase's gcc. Its Rust bindings are generated through libclang,
  and its C++ is tested only with clang. The phase's `-std=gnu11` is cut out of `CFLAGS`, because
  mozbuild sets each C file's standard itself.
- `--gcc-triple=$(gcc -dumpmachine)` — added to `CFLAGS`, `CXXFLAGS` and `LDFLAGS`. Rust's `cc`
  crate hands clang `--target=x86_64-unknown-linux-musl`, and clang then looks for gcc's headers
  and libstdc++ under that triple; naming gcc's own triple finds them. Without it the build stops
  on `'new' file not found` under `dist/system_wrappers`; see
  [An LLVM that guessed its triple](../../../../docs/kdos/05-developer/build-troubleshooting.md#an-llvm-that-guessed-its-triple).
- `-Wl,-rpath,/usr/lib/thunderbird` — a run path into the program's private library directory.
  `libxul.so` names `libmozsandbox.so`, `libgkcodecs.so` and its other siblings by bare name; the
  launcher loads them by full path first, but musl does not reuse an already-loaded library for a
  bare-name `DT_NEEDED`, so without the run path libxul fails to load; see
  [A private library loaded by full path, then not found](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-private-library-loaded-by-full-path-then-not-found).
- `RUST_TARGET=x86_64-unknown-linux-musl` — the Rust target, read by `fix-rust-target.patch`.
- `RUSTFLAGS="-C target-feature=-crt-static"` — with `RUSTC_WRAPPER`, which points at a small
  script that runs rustc with the same flag appended: link the Rust code against the shared musl.
  mach always passes cargo `--target`, and cargo then gives `RUSTFLAGS` to the target's crates
  only, so build scripts, bindgen's among them, would still link statically; every rustc call goes
  through the wrapper. See
  [Rust with a binding generator](../../../../docs/kdos/05-developer/build-troubleshooting.md#rust-with-a-binding-generator).
- `MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE=none` — mach builds its virtualenv from the packages
  vendored under `third_party/python` and installs nothing more into it, so no pip reaches the
  network. `MOZBUILD_STATE_PATH` keeps mach's state directory inside the build tree.
- `MOZ_BUILD_DATE` — the build ID, a timestamp compiled into the binary and written into every
  profile's `compatibility.ini`. Taking it from `SOURCE_DATE_EPOCH` makes two builds of the recipe
  agree.
- `--enable-application=comm/mail` — build Thunderbird, the application under `comm/`, rather than
  Firefox from the same tree.
- `--enable-default-toolkit=cairo-gtk3-x11-wayland` — compile in both of GTK 3's display backends
  and link them, so a missing X11 library is a configure error rather than a build with Wayland
  only. GDK tries Wayland first; the X11 backend serves Xwayland.
- `--enable-official-branding` — the Thunderbird name and artwork under
  `comm/mail/branding/thunderbird`, which `build.sh` installs the icons and AppStream file from.
  `--enable-update-channel=release` sets the update channel to `release` in place of upstream's
  `default`.
- `--with-distribution-id=org.kdos` — the distribution id compiled into the build, in place of
  upstream's default `org.mozilla`.
- `--with-unsigned-addon-scopes=app,system` — accept add-ons in the application and system scopes,
  the ones installed into the program's own directory and the system's extension directories,
  without a Mozilla signature.
- `--disable-bootstrap` — do not download or update toolchains; every compiler comes from the tree.
- `--disable-crashreporter` — with `--disable-updater`: no crash reporter and no self-updater; the
  program is updated as a package.
- `--disable-tests` — with `--disable-debug` and `--disable-debug-symbols`: no test programs and no
  debug information in the package.
- `--disable-strip` — with `--disable-install-strip`: Mozilla's packaging strips nothing. What the
  package keeps is the tree's size policy; see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy).
- `--disable-jemalloc` — run on musl's allocator instead of Mozilla's own mozjemalloc, which is
  unproven on musl.
- `--disable-necko-wifi` — leave out the Wi-Fi scanner of Necko, Gecko's network library.
- `--disable-cargo-incremental` — compile the Rust code without cargo's incremental cache, which
  serves only a rebuild in the same tree.
- `--without-wasm-sandboxed-libraries` — no RLBox sandboxing. It needs wasi-sdk, which the tree
  does not carry, so the libraries it would isolate (graphite, ogg, hunspell, expat, woff2,
  soundtouch) run in
  the content process as in any build without it.
- `--enable-release` — with `--enable-optimize` and `--enable-hardening`: release-engineering
  options instead of developer ones, an optimised build, and Mozilla's set of hardening compiler
  options. `--enable-release` also turns off `DEVELOPER_OPTIONS`, which is what lets
  `config/makefiles/rust.mk` run link-time optimisation on the Rust library (see
  `rust-lto-thin.patch`).
- `--with-libclang-path=/usr/lib` — where bindgen finds `libclang.so`.
- `--enable-dbus` — with `--enable-alsa` and `--enable-pulseaudio`: D-Bus support and the ALSA and
  PulseAudio sound backends, the second reaching [pipewire](#port-pipewire) through `libpulse`.
  `--disable-jack` and `--disable-sndio` leave out the JACK and sndio backends; sndio is not a
  port.
- `--with-system-nspr` — and the `--with-system-*` switches after it (nss, icu, jpeg, webp, libvpx,
  libevent, ffi, zlib, pixman, jsonc, bz2, pipewire, gbm, libdrm): link the tree's library instead
  of the copy in the source. `jsonc` and `bz2` are for librnp, the in-tree OpenPGP library, which
  runs on the in-tree Botan. PNG stays in-tree: Mozilla's configure refuses a system libpng without
  APNG support, which this one lacks. AV1 stays in-tree too, because `--with-system-av1` requires
  aom as well as [dav1d](#port-dav1d), and aom is not a port.
- `MOZILLA_OFFICIAL` — unset, together with `MOZ_TELEMETRY_REPORTING`, so no telemetry upload code
  is compiled in.
- `MAKEFLAGS` — unset after its job count is read off it: mach runs its own make, so the phase's
  `-j` is handed to `./mach build` instead and is not read twice.

## Patches
- `fix-rust-target.patch` — in `build/moz.configure/rust.configure`, take the Rust target from the `RUST_TARGET` environment variable instead of matching the C triple against `rustc --print target-list`. `x86_64-pc-linux-musl` matches both `x86_64-unknown-linux-musl` and `x86_64-unikraft-linux-musl`, and configure refuses the pair as ambiguous.
- `lfs64.patch` — use `stat` and `lstat` instead of `stat64` and `lstat64` in `xpcom/io/nsLocalFileUnix.h` and in the sandbox broker's `SandboxBrokerUtils.h`, and take the 64-bit branch in the base profiler's `shared-libraries-linux.cc`. musl has no `stat64`; its `stat` is already 64-bit.
- `musl-no-linux-prctl.patch` — drop `#include <linux/prctl.h>` from libwebrtc's `platform_thread_types.cc`, keeping `<sys/prctl.h>`. musl's `<sys/prctl.h>` and the kernel's header both define `struct prctl_mm_map`, and the second definition stops the compile.
- `rust-lto-thin.patch` — in `config/makefiles/rust.mk`, link the top-level Rust library with `-Clto=thin` instead of full LTO, and skip LTO on riscv64. Together with `glean-stub.patch` it keeps the Rust link within a build machine's memory.
- `sandbox-sched_setscheduler.patch` — allow `sched_setscheduler` whatever thread it names in the media plugin (GMP) and remote data decoder (RDD) sandbox policies. Upstream allows it only for the calling thread and answers any other thread id with `EPERM`; musl's `pthread_setschedparam` makes the call with the target thread's id.
- `fix-libresolv-path.patch` — on Linux, `comm/mail/modules/DNS.worker.mjs` looks for the resolver functions in `libc` instead of `libresolv.so.9`, `libresolv.so.2` or `libresolv.so`. musl's resolver is part of its C library, so account setup's SRV and MX lookups would otherwise find no library.
- `metainfo.patch` — add `<launchable type="desktop-id">thunderbird.desktop</launchable>` to the AppStream file, tying it to the desktop entry `build.sh` writes.

## Notes
`glean-stub.patch` carries its own description. It empties the Glean telemetry functions for
boolean, counter, event and string metrics and drops those metrics from the generated Rust code.
With telemetry compiled out the stubbed metrics record nothing either way.

After the install, `build.sh` writes `defaults/pref/kdos-prefs.js` and
`distribution/policies.json` under `/usr/lib/thunderbird`. A file under `defaults/pref` is read at
every start and a user's own `prefs.js` overrides it, so each line there is a default. The
policies (no update check, no telemetry, no add-on update) are what the settings page cannot turn
back on. The `dictionaries` and `hyphenation` directories are links to `/usr/share/hunspell` and
`/usr/share/hyphen`, from [hunspell-en](#port-hunspell-en) and [hyphen](#port-hyphen), so the
spell checker shares its dictionaries with every other program.
