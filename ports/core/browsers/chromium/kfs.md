## About

Chromium is the open-source browser Google Chrome is built from. This port builds it from the
Linux source tarball for musl, with the tree's clang, lld, Rust and Node, on GTK 3 with both the
Wayland and X11 Ozone platforms. It installs one program, `/usr/lib/chromium/chromium`, started
through the `/usr/bin/chromium` wrapper. Firefox ESR stays the default browser; Chromium claims
no MIME type.

Three sets of changes make the build possible: this port's own patches, the musl and
distribution patches of copium (a patch collection for Chromium packagers, fetched as a second
source and applied by name), and the steps in `build.sh` that put the tree's own tools and
libraries where Chromium's build would otherwise use the prebuilt copies it downloads for its
developers.

## Flags

- `${CFLAGS/-std=gnu11/}` — Removes the tree's `-std=gnu11` from `CFLAGS`. The unbundle
  toolchain appends the environment's flags last on every compile line, where the tree's standard
  would override the one Chromium picks for each source.
- `CC=clang`, `AR=llvm-ar`, `NM=llvm-nm` — With `CXX=clang++`, the programs the unbundle
  toolchain reads from the environment.
- `-Wno-unknown-warning-option`, `-Wno-builtin-macro-redefined`, `-Wno-deprecated-declarations`
  — Added to `CFLAGS` and `CXXFLAGS`: no warning for a warning switch clang does not know, for
  redefining a built-in macro, or for a call to a deprecated declaration.
- `-D__THROW=` — Defines `__THROW`, a glibc header macro that musl does not provide, as nothing,
  so declarations written with it compile.
- `RUSTC_BOOTSTRAP=1` — Lets the stable `rustc` accept the unstable features Chromium's Rust code
  uses.
- `is_official_build = true`, `is_debug = false`, `symbol_level = 0`, `blink_symbol_level = 0`
  — A release build with Chromium's official optimisation settings and no debugging symbols,
  Blink's included.
- `is_clang = true`, `clang_base_path = "/usr"`, `clang_version = "23"`,
  `clang_use_chrome_plugins = false` — Compile with the system clang. `clang_version` is the
  major version that names its resource directory, where the build finds compiler-rt; Chromium's
  own clang plugins are not part of the tree's clang.
- `is_musl = true` — An argument added by copium's `cr147-is-musl-libcxx.patch`. musl defines no
  macro by which code can detect it, so the build tells libc++ through this argument
  (`_LIBCPP_HAS_MUSL_LIBC`).
- `custom_toolchain = "//build/toolchain/linux/unbundle:default"`,
  `host_toolchain = "//build/toolchain/linux/unbundle:default"` — Chromium's toolchain for
  distributions, which takes the compiler and the `CFLAGS`, `CXXFLAGS` and `LDFLAGS` from the
  environment, for both the target and the build host.
- `use_sysroot = false` — Compile against the system's headers and libraries rather than the
  Debian sysroot Chromium downloads.
- `use_custom_libcxx = true` — Build and link the copy of libc++ in Chromium's source; Chromium's
  default.
- `use_lld = true`, `use_mold = false`, `use_siso = false` — Link with lld, not mold, and drive
  the build with ninja rather than Google's siso.
- `use_thin_lto = false`, `is_cfi = false`, `chrome_pgo_phase = 0` — No link-time optimisation, and
  none of the control-flow-integrity checks and profile-guided optimisation that an official
  Linux build turns on by default. Clang's CFI works only under LTO.
- `treat_warnings_as_errors = false`, `fatal_linker_warnings = false` — A compiler or linker
  version other than Chromium's raises warnings Chromium's does not; neither stops the build.
- `enable_nocompile_tests = false`, `blink_enable_generated_code_formatting = false` — No
  no-compile tests, and no running clang-format over Blink's generated code.
- `enable_rust = true`, `rust_bindgen_root = "/usr"` — Build Chromium's Rust code with the tree's
  Rust, and find `bindgen` in `/usr/bin`.
- `node_version_check = false` — An argument added by copium's
  `cr138-node-version-check.patch`; the build accepts the tree's Node.js, whose version differs
  from the one Chromium pins.
- `use_official_google_api_keys = false`, `google_api_key = ""`, `google_default_client_id = ""`,
  `google_default_client_secret = ""` — Build with no Google API keys, so the services that need
  them (sync and sign-in among them) are not available.
- `disable_fieldtrial_testing_config = true` — Compile in none of the field-trial testing
  configuration.
- `enable_hangout_services_extension = false`, `enable_widevine = false`, `enable_vr = false` —
  No Hangouts services extension, no Widevine support for protected video, and no WebXR.
- `safe_browsing_use_unrar = false` — Build without `third_party/unrar`, which Chromium allows
  leaving out for licence reasons; `build.sh` deletes that source as well.
- `proprietary_codecs = true`, `ffmpeg_branding = "Chrome"` — Decode the patent-encumbered
  formats, H.264 and AAC among them, through the bundled FFmpeg.
- `ozone_platform_wayland = true`, `ozone_platform_x11 = true` — Compile both display platforms.
  The wrapper passes `--ozone-platform-hint=auto`, which picks Wayland in the session.
- `use_qt5 = false`, `use_qt6 = false` — No Qt platform integration; GTK 3 alone.
- `use_gio = true`, `use_cups = true`, `use_kerberos = true`, `use_vaapi = true` — GIO,
  printing through [cups](#port-cups), Kerberos authentication through [krb5](#port-krb5), and
  VA-API video decoding through [libva](#port-libva).
- `use_pulseaudio = true`, `link_pulseaudio = true` — Sound through the PulseAudio API, linked
  with `DT_NEEDED` rather than loaded with `dlopen`, so a missing or mismatched library shows at
  load time.
- `rtc_use_pipewire = true`, `rtc_link_pipewire = true` — Screen capture for WebRTC through
  PipeWire, linked directly the same way.
- `use_system_libffi = true` — Link the tree's [libffi](#port-libffi).
- `icu_use_data_file = true` — Load ICU's data from `icudtl.dat` beside the program instead of
  linking it in.

## Patches

- `kdos-toolchain.patch` — Fits Chromium's build to this toolchain. It looks for compiler-rt in `lib/linux` with an architecture suffix (`libclang_rt.builtins-x86_64.a`), the layout the tree's compiler-rt installs, instead of a per-triple directory; it targets `x86_64-pc-linux-musl` in C and C++ and `x86_64-unknown-linux-musl` in Rust, and adds the latter to `known-target-triples.txt`; it passes `-Ctarget-feature=-crt-static` to rustc, because Rust's musl target links statically and this Rust code links into dynamically linked programs; and it drops some compiler flags: `-fdiagnostics-show-inlining-chain`, `-fno-lifetime-dse`, the `-split-threshold-for-reg-with-hint` option, `-fsanitize-ignore-for-ubsan-feature`, and the `-Wall` on Chromium's own code.
- `disable-dns_config_service.patch` — On Linux, compiles `dns_config_service_fuchsia.cc` in place of `dns_config_service_linux.cc`, and drops the Linux unit test. The Linux service reads `resolv.conf` and `nsswitch.conf` with glibc's resolver state; the Fuchsia one is a stub that reads and watches nothing.
- `musl-sandbox.patch` — Fits the seccomp sandbox to the system calls musl makes. A thread `clone` is allowed whatever combination of `CLONE_SETTLS`, `CLONE_PARENT_SETTID`, `CLONE_CHILD_CLEARTID` and `CLONE_DETACHED` it carries, not only glibc's exact set; `set_tid_address`, `mremap` and `membarrier` join the baseline's allowed sets; the renderer may call `pwritev2` and the three scheduler calls; and the baseline policy allows `sched_getparam`, `sched_getscheduler` and `sched_setscheduler` outright.
- `musl-conditional-tkill.patch` — Lets a renderer call `tkill`, which musl's `pthread_kill` uses and V8 calls while profiling, but only when the renderer is PID 1 of its own PID namespace with the user-namespace sandbox, where a signal can reach only its own threads.
- `musl-tid-caching.patch` — After the sandbox forks a process into a new PID namespace, writes the real thread id into musl's thread structure, as Chromium already does for glibc's cache. musl's structure is not public, so the patch declares a copy of its layout.
- `no-execinfo.patch` — Compiles out every use of `<execinfo.h>`, `backtrace()` and `backtrace_symbols()` when the C library is not glibc: in V8, in SwiftShader's LLVM configuration and in `base/debug/stack_trace.cc`, which then prints no stack trace.
- `no-mallinfo.patch` — Compiles out the uses of `mallinfo()` and `mallinfo2()`, which musl's allocator does not provide, in Chromium's memory dump and process metrics, TensorFlow Lite's profiler and SwiftShader's LLVM, and drops the `mallinfo()` that PartitionAlloc's allocator shim exports in their place. The process metrics then report a malloc usage of 0.
- `no-res-ninit-nclose.patch` — `ScopedResState` uses the global `res_init` path rather than `res_ninit`, and its destructor calls no `res_nclose`; musl has neither function.
- `no-sandbox-settls.patch` — Turns off the `CLONE_VM | CLONE_VFORK | CLONE_SETTLS` path in `sandbox/linux/services/credentials.cc`, so that `clone` runs without them. The patch's own note gives the reason: that use of `CLONE_SETTLS` is invalid, and musl's `clone()` is to refuse it with `EINVAL`.
- `temp-failure-retry.patch` — Defines glibc's `TEMP_FAILURE_RETRY` macro, which retries a call that failed with `EINTR`, in the setuid sandbox's `process_util.h`; musl has no such macro.
- `unbundle-official-build.patch` — The shim headers that stand in for bundled libraries refuse to compile in an official build; the patch ties that check to a Google Chrome build instead, so an official build with system libraries works.
- `rust-cbor.patch` — Debian's patch: the Rust CBOR parser, and its Crubit bindings, are used only when `enable_cpp_api_from_rust` is set, instead of in every build that is not Cronet.
- `gnrt-no-tls.patch` — Builds `gnrt`'s `reqwest` dependency without its default features, so no TLS stack is among the crates the tool needs; `gnrt` makes no network request here.
- `protobuf-pure-python.patch` — Adds `_api_implementation.py` to Chromium's protobuf Python package, choosing the pure-Python implementation before protobuf probes for a compiled one. The system's compiled `google._upb` belongs to another protobuf release and fails against this tree's `google.protobuf.message`.

## Notes

The source tarball carries prebuilt programs for Chromium's developers. Before anything is
configured, `build.sh` deletes every ELF file in the tree, then links the tree's own `node`,
`gperf`, `go`, `esbuild`, `clang-format`, `cargo`, `rustc`, `rustfmt` and Crubit's
`cc_bindings_from_rs`, the Rust standard library and Crubit's support library, and a copy of
`typescript-go`'s `tsc`, into the places the build looks for its downloaded toolchains. It writes
`rustc -V` into `third_party/rust-toolchain/VERSION`, which the GN files read as the toolchain's
revision. A tool this step does not link is a tool the build cannot find; the `esbuild` and
`crubit` ports are pinned to the versions this Chromium names (see
[the ports catalogue](../../../../docs/kdos/06-reference/ports-catalogue.md)).

Chromium compiles Rust's standard library itself, from GN rules that its `gnrt` tool generates.
`gnrt` is a Rust program in `tools/crates/gnrt`, and its crates are the port's vendor bundle
(`vendoring = rust`, `vendordir = tools/crates/gnrt`, see
[vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)): `build.sh` unpacks
the bundle there, builds `gnrt` offline, and runs `gnrt gen --for-std` against the Rust source the
tree's Rust installs.

The libraries in `_system` are taken from their ports: `build.sh` deletes the bundled copies'
sources, keeping their GN files, and `build/linux/unbundle/replace_gn_files.py` points the build
at the system's. ffmpeg, ICU, libxml and the rest stay bundled.

`ulimit -n` is raised to 4096 (or the hard limit) before ninja runs. Only the `chrome` and
`chrome_crashpad_handler` targets are built, and the install copies the program, its resource
packs, the ANGLE, Vulkan loader and SwiftShader libraries and the `en-US` and `en-GB` locale packs alone.

The `/usr/bin/chromium` wrapper sets the three Google API variables to `no`. With none
configured, Chromium shows an infobar about missing API keys at every start. It adds the
contents of `$CHROMIUM_FLAGS` to the command line. The managed policy file,
`/etc/chromium/policies/managed/kdos.json`, turns off metrics, component updates, Safe Browsing,
password leak detection, sign-in and sync, spell checking and its network service, translation,
search suggestions, network prediction, DNS over HTTPS, background mode and the generative AI
features.
