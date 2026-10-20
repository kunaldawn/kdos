# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

for p in 0001-hotfix-ignore-a-new-warning-in-rust-1.89 kdos-toolchain \
	disable-dns_config_service musl-sandbox musl-conditional-tkill \
	musl-tid-caching no-execinfo \
	no-mallinfo no-res-ninit-nclose no-sandbox-settls temp-failure-retry \
	rust-cbor unbundle-official-build gnrt-no-tls protobuf-pure-python; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done

for p in cr138-node-version-check cr140-musl-prctl \
	cr143-libsync-__BEGIN_DECLS cr145-iwyu-dev_t \
	cr145-musl-unfortify-SkDescriptor \
	cr146-swiftshader-unfortify-memset-memcpy \
	cr146-unfortify-blink-display_item_list cr147-is-musl-libcxx \
	cr148-rust-1.95-bytemuck cr148-v8-no-san-trap \
	cr149-musl-alloc-shim-dispatch cr149-rust-toolchain-var \
	cr151-dawn-cgo_enabled cr152-unbundle-opus-devtools \
	cr155-no-python-vendoring cr153-typescript-break-definitions \
	cr152-unbundle-minizip-undo-unicode cr153-unbundle-opus-iamf \
	cr155-devtools-typescript-6.x-isolated-declarations; do
	patch -p1 -i "$SRC_ROOT/copium/$p.patch"
done

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
install -m755 /usr/lib/typescript-go/tsc third_party/typescript/linux-amd64/src/lib/tsc
for _arch in amd64 ""; do
	install -d third_party/dawn/tools/golang/linux-$_arch/bin
	ln -sf /usr/bin/go third_party/dawn/tools/golang/linux-$_arch/bin/go
done

install -d third_party/rust-toolchain/bin third_party/rust-toolchain/lib/third_party
for _t in cargo rustc rustfmt cc_bindings_from_rs; do
	ln -sf /usr/bin/$_t third_party/rust-toolchain/bin/$_t
done
ln -sfn /usr/lib/rustlib third_party/rust-toolchain/lib/rustlib
ln -sfn /usr/share/crubit third_party/rust-toolchain/lib/third_party/crubit
install -d buildtools/linux64-format
ln -sf /usr/bin/clang-format buildtools/linux64-format/clang-format
rustc -V > third_party/rust-toolchain/VERSION
cp third_party/rust-toolchain/VERSION third_party/rust-toolchain/INSTALLED_VERSION

(
	cd tools/crates/gnrt
	tar xf "$PORT_SRC/$name-vendor-$version.tar.xz"
	RUSTFLAGS="-C target-feature=-crt-static" cargo build --release --offline
)
tools/crates/gnrt/target/release/gnrt gen --for-std third_party/rust-toolchain/lib/rustlib/src/rust

ln -sf /usr/bin/esbuild third_party/devtools-frontend/src/third_party/esbuild/esbuild

_system="brotli dav1d double-conversion flac fontconfig freetype harfbuzz
	libdrm libjpeg libsecret libusb libwebp openh264 opus zlib zstd"
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

export CC=clang CXX=clang++ AR=llvm-ar NM=llvm-nm
export CFLAGS="${CFLAGS/-std=gnu11/} -Wno-unknown-warning-option -Wno-builtin-macro-redefined -Wno-deprecated-declarations -D__THROW="
export CXXFLAGS="$CXXFLAGS -Wno-unknown-warning-option -Wno-builtin-macro-redefined -Wno-deprecated-declarations -D__THROW="
export RUSTC_BOOTSTRAP=1

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
    v8_metagen_libclang_so = "/usr/lib/libclang.so"
    v8_metagen_libclang_bindings_dir = "../../third_party/llvm-libclang/bindings/python"
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

install -Dm644 out/Release/locales/en-US.pak "$PKG$_app/locales/en-US.pak"
install -Dm644 out/Release/locales/en-GB.pak "$PKG$_app/locales/en-GB.pak"

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
