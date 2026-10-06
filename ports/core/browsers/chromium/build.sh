# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# CHROMIUM ON ITS OWN TOOLCHAIN: clang 23 and lld, Chromium's bundled libc++
# (use_custom_libcxx; the system C++ runtime is libstdc++, and a system C++
# library would have to share Chromium's libc++ ABI to be unbundled), and the
# system rustc with its prebuilt musl standard library. Ozone carries both the
# Wayland and the X11 platforms; --ozone-platform-hint=auto in the launcher
# picks Wayland on kdos-comp and X11 under Xwayland alone.
#
# Alpine's musl series, verbatim. kdos-toolchain is Alpine's compiler.patch
# with this tree's triples: clang's x86_64-pc-linux-musl, rustc's
# x86_64-unknown-linux-musl (added to the triples gn accepts), and
# compiler-rt's /usr/lib/clang/23/lib/linux layout; it also drops three clang
# flags clang 23 does not know and turns off crt-static, which rustc's musl
# targets default to. unbundle-official-build lets the shim headers of the
# system libraries compile in an official build. protobuf-pure-python pins
# the bundled Python protobuf, which the build's generators import, to its
# pure-Python implementation: left to probe, it loads the system protobuf's
# compiled module, a different release that fails against this tree's.
for p in 0001-hotfix-ignore-a-new-warning-in-rust-1.89 kdos-toolchain \
	disable-dns_config_service musl-sandbox musl-tid-caching no-execinfo \
	no-mallinfo no-res-ninit-nclose no-sandbox-settls temp-failure-retry \
	rust-cbor unbundle-official-build gnrt-no-tls protobuf-pure-python; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done

# copium is the patch set Alpine applies for this milestone, a later source
# unpacked beside the tree. Left out: the fixes for a rustc older than this
# tree's, among them the crubit ones (crubit is the crubit port's), the FFmpeg
# and simdutf fixes (those libraries stay bundled), the standard library
# source fix (this rust-src has the crates it restores) and the armv7 one. The system zlib brings the system minizip with it, which lacks
# Chromium's Unicode path field, so its zip reader is taken back to the plain
# name.
for p in cr138-node-version-check cr140-musl-prctl \
	cr143-libsync-__BEGIN_DECLS cr145-iwyu-dev_t \
	cr145-musl-unfortify-SkDescriptor \
	cr146-swiftshader-unfortify-memset-memcpy \
	cr146-unfortify-blink-display_item_list cr147-is-musl-libcxx \
	cr148-rust-1.95-bytemuck cr148-v8-no-san-trap \
	cr149-musl-alloc-shim-dispatch cr149-rust-toolchain-var \
	cr151-dawn-cgo_enabled cr152-unbundle-opus-devtools \
	cr153-no-python-vendoring cr153-typescript-break-definitions \
	cr152-unbundle-minizip-undo-unicode \
	cr153-unbundle-opus-iamf cr154-devtools-fe-ai-assistance-skills \
	cr154-devtools-no-typecheck; do
	patch -p1 -i "$SRC_ROOT/copium/$p.patch"
done

# The tarball carries prebuilt glibc tools and node addons (gperf, esbuild,
# rollup's native module, a Python). None runs on musl and none was compiled
# here, so every ELF file goes, and each tool the build calls is this tree's.
python3 - <<'PY'
import os
for root, dirs, files in os.walk('.'):
    for f in files:
        p = os.path.join(root, f)
        if os.path.islink(p) or not os.path.isfile(p):
            continue
        with open(p, 'rb') as fh:
            if fh.read(4) == b'\x7fELF':
                os.remove(p)
PY
install -d third_party/node/linux/node-linux-x64/bin third_party/gperf/cipd/bin
ln -sf /usr/bin/node third_party/node/linux/node-linux-x64/bin/node
ln -sf /usr/bin/gperf third_party/gperf/cipd/bin/gperf
# TypeScript is the Go compiler, whose npm build reads lib.*.d.ts from beside
# its own resolved path: the tree keeps Chromium's patched set in src/lib, so
# a copy of the binary goes there, where a link would read the port's own set.
install -m755 /usr/lib/typescript-go/tsc third_party/typescript/linux-amd64/src/lib/tsc
for _arch in amd64 ""; do
	install -d third_party/dawn/tools/golang/linux-$_arch/bin
	ln -sf /usr/bin/go third_party/dawn/tools/golang/linux-$_arch/bin/go
done

# THE RUST TOOLCHAIN, LAID OUT AS CHROMIUM'S OWN. Chromium calls Rust from C++
# through headers crubit's cc_bindings_from_rs writes, and Blink and the
# browser both do; GN enables that only with its own toolchain, whose standard
# library it compiles from source. third_party/rust-toolchain is therefore the
# system rustc, cargo and rustfmt, the system rustlib (whose src/rust is the
# library source and its vendored crates), and the crubit port's tool and
# support tree. VERSION is what GN keys rebuilds of the Rust targets on.
install -d third_party/rust-toolchain/bin third_party/rust-toolchain/lib/third_party
for _t in cargo rustc rustfmt cc_bindings_from_rs; do
	ln -sf /usr/bin/$_t third_party/rust-toolchain/bin/$_t
done
ln -sfn /usr/lib/rustlib third_party/rust-toolchain/lib/rustlib
ln -sfn /usr/share/crubit third_party/rust-toolchain/lib/third_party/crubit
# cc_bindings_from_rs formats the headers it writes with the clang-format
# buildtools would hold; the clang port's is that tool.
install -d buildtools/linux64-format
ln -sf /usr/bin/clang-format buildtools/linux64-format/clang-format
rustc -V > third_party/rust-toolchain/VERSION
cp third_party/rust-toolchain/VERSION third_party/rust-toolchain/INSTALLED_VERSION

# The GN rules for the standard library are generated for the toolchain's own
# library source: the tarball's are for Chromium's rustc, whose library pins
# other versions of its crates. gnrt writes them, built from the vendored
# crates its lock file names. gnrt-no-tls drops reqwest's default TLS
# backend, which only gnrt's crate downloads use: it reaches OpenSSL through
# an openssl-sys that refuses OpenSSL 4. The lock file then names crates the
# build no longer needs, so cargo runs --offline without --frozen and prunes
# them.
(
	cd tools/crates/gnrt
	tar xf "$PORT_SRC/$name-vendor-$version.tar.xz"
	RUSTFLAGS="-C target-feature=-crt-static" cargo build --release --offline
)
tools/crates/gnrt/target/release/gnrt gen --for-std third_party/rust-toolchain/lib/rustlib/src/rust

# DevTools bundles its front end with esbuild and rollup. esbuild's JavaScript
# API in node_modules refuses a binary of any version but its own, so the
# esbuild port is held at the version node_modules/esbuild names. rollup's
# native module is a glibc addon; @rollup/wasm-node is the same bundler in
# WebAssembly and takes its place.
ln -sf /usr/bin/esbuild third_party/devtools-frontend/src/third_party/esbuild/esbuild
rm -rf third_party/devtools-frontend/src/node_modules/rollup
cp -a "$SRC_ROOT/package" third_party/devtools-frontend/src/node_modules/rollup

# System libraries, through Chromium's own unbundling: the bundled sources are
# deleted so nothing can include them by accident, and the BUILD files are
# swapped for the shims in build/linux/unbundle. FFmpeg (the system major is
# newer than Chromium's media code takes), ICU, libxml2 and libxslt stay
# bundled, as do the C++ libraries the bundled libc++ cannot share (re2,
# snappy, jsoncpp, woff2) and those with no port (openh264, crc32c).
_system="brotli dav1d double-conversion flac fontconfig freetype harfbuzz
	libdrm libjpeg libsecret libusb libwebp opus zlib zstd"
for _lib in $_system libjpeg_turbo unrar; do
	find . -type f -path "*third_party/$_lib/*" \
		\! -path "*third_party/$_lib/chromium/*" \
		\! -path "*third_party/$_lib/google/*" \
		\! -path './base/third_party/icu/*' \
		\! -path './third_party/libxml/*' \
		\! -path './third_party/pdfium/third_party/freetype/include/pstables.h' \
		\! -path './third_party/crashpad/crashpad/third_party/zlib/zlib_crashpad.h' \
		\! -regex '.*\.\(gn\|gni\|isolate\|py\)' \
		-delete
done
python3 build/linux/unbundle/replace_gn_files.py --system-libraries $_system

# The unbundle toolchain takes the compilers and every *FLAGS from the
# environment and appends them last, so the phase's -std=gnu11 would override
# the C standard Chromium picks. The build runs nightly-only rustc features
# that Chromium's own toolchain allows; RUSTC_BOOTSTRAP grants them to the
# stable rustc. musl declares malloc and the rest with no exception
# specification; PartitionAlloc's shim declares them again with __THROW, which
# it defines as noexcept when the C library has not defined it, and the two
# declarations then disagree. -D__THROW= is the empty definition the shim
# expects from a sys/cdefs.h; libbsd's, the one here, has none.
export CC=clang CXX=clang++ AR=llvm-ar NM=llvm-nm
export CFLAGS="${CFLAGS/-std=gnu11/} -Wno-unknown-warning-option -Wno-builtin-macro-redefined -Wno-deprecated-declarations -D__THROW="
export CXXFLAGS="$CXXFLAGS -Wno-unknown-warning-option -Wno-builtin-macro-redefined -Wno-deprecated-declarations -D__THROW="
export RUSTC_BOOTSTRAP=1

# No Google API keys: without them sync, sign-in, Safe Browsing, translation
# and the location service do nothing, which offline they could not do
# anyway. Widevine is off (a closed module fetched at run time), as are the
# field-trial config, the Hangouts extension, the VR runtime and unrar.
# Profile-guided optimisation needs a profile gclient downloads, and ThinLTO
# multiplies the link's memory; both are off. Qt integration is off: the
# browser draws with GTK 3, loaded at run time. GN rejects a tab anywhere in
# --args, so the list is indented with spaces.
gn gen out/Release --args='
    is_official_build = true
    is_debug = false
    symbol_level = 0
    blink_symbol_level = 0
    is_clang = true
    is_musl = true
    clang_base_path = "/usr"
    clang_version = "23"
    clang_use_chrome_plugins = false
    custom_toolchain = "//build/toolchain/linux/unbundle:default"
    host_toolchain = "//build/toolchain/linux/unbundle:default"
    use_sysroot = false
    use_custom_libcxx = true
    use_lld = true
    use_mold = false
    use_siso = false
    use_thin_lto = false
    is_cfi = false
    chrome_pgo_phase = 0
    treat_warnings_as_errors = false
    fatal_linker_warnings = false
    enable_nocompile_tests = false
    blink_enable_generated_code_formatting = false
    enable_rust = true
    rust_bindgen_root = "/usr"
    node_version_check = false
    use_official_google_api_keys = false
    google_api_key = ""
    google_default_client_id = ""
    google_default_client_secret = ""
    disable_fieldtrial_testing_config = true
    enable_hangout_services_extension = false
    enable_widevine = false
    enable_vr = false
    safe_browsing_use_unrar = false
    proprietary_codecs = true
    ffmpeg_branding = "Chrome"
    ozone_platform_wayland = true
    ozone_platform_x11 = true
    use_qt5 = false
    use_qt6 = false
    use_gio = true
    use_cups = true
    use_kerberos = true
    use_vaapi = true
    use_pulseaudio = true
    link_pulseaudio = true
    rtc_use_pipewire = true
    rtc_link_pipewire = true
    use_system_libffi = true
    icu_use_data_file = true
'

# The final link holds thousands of objects open at once; the default soft
# limit of 1024 descriptors stops it; where the hard limit is lower than 4096,
# the hard limit is what there is. MAKEFLAGS is ninja's only job limit, and
# unbounded ninja runs a compiler per core at up to 2 GB each.
ulimit -n 4096 2>/dev/null || ulimit -n "$(ulimit -Hn)"
ninja -C out/Release $MAKEFLAGS chrome chrome_crashpad_handler

_app=/usr/lib/chromium
install -Dm755 out/Release/chrome "$PKG$_app/chromium"
install -Dm755 out/Release/chrome_crashpad_handler "$PKG$_app/chrome_crashpad_handler"
for f in chrome_100_percent.pak chrome_200_percent.pak resources.pak \
	icudtl.dat v8_context_snapshot.bin libEGL.so libGLESv2.so \
	libvulkan.so.1 libvk_swiftshader.so vk_swiftshader_icd.json; do
	install -m644 "out/Release/$f" "$PKG$_app/$f"
done
chmod 755 "$PKG$_app"/*.so "$PKG$_app"/*.so.1

# English only: the browser falls back to en-US for any other locale.
install -Dm644 out/Release/locales/en-US.pak "$PKG$_app/locales/en-US.pak"
install -Dm644 out/Release/locales/en-GB.pak "$PKG$_app/locales/en-GB.pak"

# The launcher names the desktop entry, so the Wayland app_id and the X11
# class are `chromium`, and sets the three key variables to a value that is
# not the unset marker, so no "Google API keys are missing" bar is drawn on
# every start. CHROMIUM_FLAGS in the environment adds switches.
install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/chromium" <<'KDOS_SH'
#!/bin/sh
export CHROME_WRAPPER=/usr/bin/chromium
export CHROME_DESKTOP=chromium.desktop
export GOOGLE_API_KEY=no
export GOOGLE_DEFAULT_CLIENT_ID=no
export GOOGLE_DEFAULT_CLIENT_SECRET=no
exec /usr/lib/chromium/chromium --ozone-platform-hint=auto $CHROMIUM_FLAGS "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/chromium"

# The lockdown, read from /etc/chromium/policies/managed and winning over
# every profile: no usage or crash reporting, no component updater (it would
# fetch CRLSets, Widevine and the rest from Google), no Safe Browsing lookups,
# no DNS over HTTPS, no password-leak lookups, no search suggestions,
# translation, network prediction, Google sign-in or sync, and no
# generative-AI features. The spelling
# dictionaries are downloads too, so the spelling service is off.
install -d "$PKG/etc/chromium/policies/managed"
cat > "$PKG/etc/chromium/policies/managed/kdos.json" <<'EOF'
{
  "MetricsReportingEnabled": false,
  "UrlKeyedAnonymizedDataCollectionEnabled": false,
  "ComponentUpdatesEnabled": false,
  "SafeBrowsingProtectionLevel": 0,
  "PasswordLeakDetectionEnabled": false,
  "DnsOverHttpsMode": "off",
  "SearchSuggestEnabled": false,
  "AlternateErrorPagesEnabled": false,
  "TranslateEnabled": false,
  "NetworkPredictionOptions": 2,
  "SpellCheckServiceEnabled": false,
  "SpellcheckEnabled": false,
  "BrowserSignin": 0,
  "SyncDisabled": true,
  "PromotionsEnabled": false,
  "DefaultBrowserSettingEnabled": false,
  "BackgroundModeEnabled": false,
  "GenAiDefaultSettings": 2
}
EOF
chmod 644 "$PKG/etc/chromium/policies/managed/kdos.json"

for _s in 24 48 64 128 256; do
	install -Dm644 "chrome/app/theme/chromium/product_logo_$_s.png" \
		"$PKG/usr/share/icons/hicolor/${_s}x${_s}/apps/chromium.png"
done

# No MimeType: firefox-esr claims the web types, and mimeapps.list is where
# the default browser is chosen.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/chromium.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Chromium
GenericName=Web Browser
Comment=Browse the World Wide Web
Exec=chromium %U
Icon=chromium
Terminal=false
StartupNotify=true
StartupWMClass=chromium
Categories=Network;WebBrowser;
Keywords=web;browser;internet;www;http;chromium;chrome;
Actions=new-window;new-private-window;

[Desktop Action new-window]
Name=New Window
Exec=chromium --new-window %U

[Desktop Action new-private-window]
Name=New Incognito Window
Exec=chromium --incognito %U
EOF
chmod 644 "$PKG/usr/share/applications/chromium.desktop"

install -Dm644 LICENSE "$PKG/usr/share/licenses/$name/LICENSE"
