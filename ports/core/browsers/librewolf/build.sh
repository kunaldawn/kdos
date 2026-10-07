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


# LIBREWOLF ON GTK 3, WITH BOTH GDK BACKENDS, built with the toolchain and the
# musl patches firefox-esr uses. The source is Firefox with LibreWolf's patches
# already applied; its lw/ directory installs librewolf.cfg, the
# local-settings.js that loads it, and distribution/policies.json, and this
# script replaces only the last. cairo-gtk3-x11-wayland makes both halves
# required, so a missing X client library stops configure instead of leaving a
# browser that cannot start under Xwayland; GDK tries Wayland first.
#
# The binary, /usr/lib/librewolf, the desktop entry and the Wayland app_id
# (MOZ_APP_REMOTINGNAME) all carry the one name librewolf, and StartupWMClass
# has to equal it or the panel groups the window under no entry.
#
# Five of Alpine's firefox patches: lfs64, musl-no-linux-prctl,
# sandbox-sched_setscheduler, glean-stub and rust-lto-thin, each for the reason
# firefox-esr's build.sh gives; rust-lto-thin is rebased onto this tree's
# rust.mk, the rest are verbatim.
# fix-rust-target takes the rustc target from RUST_TARGET:
# configure matches the C triple against rustc's target list, where a musl
# x86_64 triple fits both x86_64-unknown-linux-musl and
# x86_64-unikraft-linux-musl and the vendor `kdos` picks neither, so
# configure stops. musl-pthread-t: audio_thread_priority serialises a
# pthread_t through the integer to_ne_bytes, and musl's pthread_t is a pointer;
# the patch goes through usize, the same width for either, and updates the
# crate's .cargo-checksum.json, which cargo checks every vendored file against.
patch -p1 -i "$PORT_SRC/fix-rust-target.patch"
patch -p1 -i "$PORT_SRC/musl-pthread-t.patch"
patch -p1 -i "$PORT_SRC/glean-stub.patch"
patch -p1 -i "$PORT_SRC/lfs64.patch"
patch -p1 -i "$PORT_SRC/musl-no-linux-prctl.patch"
patch -p1 -i "$PORT_SRC/rust-lto-thin.patch"
patch -p1 -i "$PORT_SRC/sandbox-sched_setscheduler.patch"

# Clang and lld, as for firefox-esr; bindgen needs libclang whichever compiler
# builds the C++. The phase's -std=gnu11 is dropped: mozbuild sets the
# language standard of every C file itself. The zero-initialised stack and
# wrapping signed overflow are LibreWolf's own hardening flags. The cc crate
# compiles with --target=x86_64-unknown-linux-musl, Rust's triple, and clang
# looks for gcc's headers and libraries under the triple it is given;
# --gcc-triple names gcc's own, so libstdc++ is found under either.
_gcc_triple="--gcc-triple=$(gcc -dumpmachine)"
export CC=clang
export CXX=clang++
export CFLAGS="-ftrivial-auto-var-init=zero -fwrapv ${CFLAGS/-std=gnu11/} $_gcc_triple"
export CXXFLAGS="-ftrivial-auto-var-init=zero -fwrapv $CXXFLAGS $_gcc_triple"
export LDFLAGS="$LDFLAGS $_gcc_triple"

# libxul names libmozsandbox.so, libgkcodecs.so and the rest of its siblings
# by bare name. The launcher loads them from /usr/lib/librewolf first, but
# musl does not take an already-loaded library for a bare-name DT_NEEDED, so
# without a run path into that directory libxul fails to load.
export LDFLAGS="$LDFLAGS -Wl,-rpath,/usr/lib/librewolf"

# rustc's musl targets default to crt-static; gkrust is linked into the
# shared libxul, which takes the shared C library like every other port.
# mach always passes cargo --target, and cargo then gives RUSTFLAGS to the
# target's crates only: build scripts would still link statically, and
# bindgen's, which dlopens libclang, would fail. cargo runs every rustc call,
# host and target, through RUSTC_WRAPPER, which adds the flag to each.
export RUSTFLAGS="-C target-feature=-crt-static"
export RUST_TARGET=x86_64-unknown-linux-musl
cat > "$SRC/rustc-dynamic" <<'EOF'
#!/bin/sh
rustc=$1
shift
exec "$rustc" "$@" -C target-feature=-crt-static
EOF
chmod 755 "$SRC/rustc-dynamic"
export RUSTC_WRAPPER="$SRC/rustc-dynamic"

# mach would pip-install zstandard and psutil into its virtualenv from the
# network; `none` builds with the pure-Python copies in the tarball.
export MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE=none
export MOZBUILD_STATE_PATH="$SRC/.mozbuild"
export MOZ_NOSPAM=1
export MOZ_BUILD_DATE="$(date -u -d "@$SOURCE_DATE_EPOCH" +%Y%m%d%H%M%S)"

# The tarball's own mozconfig is not used: it turns jemalloc and
# MOZILLA_OFFICIAL on. The options below are firefox-esr's, with LibreWolf's
# branding in place of the official one. MOZ_TELEMETRY_REPORTING is off at
# build time and DisableTelemetry holds it off at run time. An empty
# MOZ_REQUIRE_SIGNING leaves add-on signature checks to the
# xpinstall.signatures.required pref, as LibreWolf ships them.
#
# --without-wasm-sandboxed-libraries: RLBox needs a wasm32-wasi sysroot and a
# rustc with the WebAssembly target, and this tree's rustc is built for X86
# alone. --disable-eme: the only module is Widevine, a glibc binary fetched at
# run time. --disable-jemalloc: mozjemalloc is unproven on musl.
# --disable-necko-wifi: the Wi-Fi scanner feeds only network geolocation.
# English only: lw/l10n is never named, so no locale is repacked.
cat > "$SRC/kdos.mozconfig" <<EOF
ac_add_options --prefix=/usr
ac_add_options --with-app-name=librewolf
ac_add_options --with-branding=browser/branding/librewolf
ac_add_options MOZ_APP_REMOTINGNAME=librewolf
ac_add_options --with-distribution-id=org.kdos
ac_add_options MOZ_TELEMETRY_REPORTING=
ac_add_options MOZ_REQUIRE_SIGNING=

ac_add_options --enable-application=browser
ac_add_options --enable-default-toolkit=cairo-gtk3-x11-wayland
ac_add_options --enable-release
ac_add_options --enable-optimize=-O2
ac_add_options --enable-hardening
ac_add_options --enable-stl-hardening
ac_add_options --enable-linker=lld
ac_add_options --with-libclang-path=/usr/lib
ac_add_options --without-wasm-sandboxed-libraries

ac_add_options --disable-bootstrap
ac_add_options --disable-cargo-incremental
ac_add_options --disable-crashreporter
ac_add_options --disable-updater
ac_add_options --disable-eme
ac_add_options --disable-jemalloc
ac_add_options --disable-necko-wifi
ac_add_options --disable-debug
ac_add_options --disable-debug-symbols
ac_add_options --disable-tests
ac_add_options --disable-strip
ac_add_options --disable-install-strip

ac_add_options --enable-dbus
ac_add_options --enable-alsa
ac_add_options --enable-pulseaudio
ac_add_options --disable-jack
ac_add_options --disable-sndio

ac_add_options --with-system-nspr
ac_add_options --with-system-nss
ac_add_options --with-system-icu
ac_add_options --with-system-ffi
ac_add_options --with-system-libevent
ac_add_options --with-system-jpeg
ac_add_options --with-system-libvpx
ac_add_options --with-system-webp
ac_add_options --with-system-zlib
ac_add_options --with-system-pixman
ac_add_options --with-system-pipewire
ac_add_options --with-system-gbm
ac_add_options --with-system-libdrm

mk_add_options MOZ_OBJDIR=$SRC/obj
mk_add_options MOZ_CRASHREPORTER=0
mk_add_options MOZ_DATA_REPORTING=0
mk_add_options MOZ_SERVICES_HEALTHREPORT=0
mk_add_options MOZ_TELEMETRY_REPORTING=0
EOF
export MOZCONFIG="$SRC/kdos.mozconfig"

./mach build
DESTDIR="$PKG" ./mach install

_app=/usr/lib/librewolf

if [ -f "$PKG$_app/librewolf-bin" ] && [ ! -L "$PKG$_app/librewolf-bin" ]; then
	ln -sf librewolf "$PKG$_app/librewolf-bin"
fi

# LibreWolf's policies, less what needs the network at run time, plus the
# lockdown firefox-esr ships. Its own file installs uBlock Origin from
# addons.mozilla.org on first start, which offline retries at every start and
# never succeeds, so no extension is force-installed; language packs stay
# refused. Remote Settings, DNS over HTTPS, captive-portal probes, Mozilla
# accounts, online suggestions and the AI features are off. Disabling Remote
# Settings also stops blocklist and CRLite revocation updates; the copies
# packaged with this release are what is enforced.
install -d "$PKG$_app/distribution"
cat > "$PKG$_app/distribution/policies.json" <<'EOF'
{
  "policies": {
    "DisableTelemetry": true,
    "DisableFirefoxStudies": true,
    "DisableRemoteImprovements": true,
    "DisableRemoteSettingsAndAcceptSecurityConsequences": true,
    "DisableAppUpdate": true,
    "AppUpdateURL": "https://localhost",
    "DisableSystemAddonUpdate": true,
    "DisableDefaultBrowserAgent": true,
    "DisableFeedbackCommands": true,
    "DisableFirefoxAccounts": true,
    "DontCheckDefaultBrowser": true,
    "CaptivePortal": false,
    "DNSOverHTTPS": { "Enabled": false, "Locked": true },
    "NetworkPrediction": false,
    "SearchSuggestEnabled": false,
    "TranslateEnabled": false,
    "IPProtectionAvailable": false,
    "SkipTermsOfUse": true,
    "NoDefaultBookmarks": true,
    "HttpsOnlyMode": "enabled",
    "EncryptedMediaExtensions": { "Enabled": false },
    "LocalNetworkAccess": {
      "Enabled": true,
      "BlockTrackers": true,
      "EnablePrompting": true
    },
    "OverrideFirstRunPage": "",
    "OverridePostUpdatePage": "",
    "GenerativeAI": { "Enabled": false, "Locked": true },
    "AIControls": { "Default": { "Value": "blocked", "Locked": true } },
    "ExtensionSettings": {
      "*": {
        "blocked_install_message": "LibreWolf does not allow installing Language Packs.",
        "installation_mode": "allowed",
        "allowed_types": ["dictionary", "extension", "sitepermission", "theme"]
      }
    },
    "FirefoxHome": {
      "Weather": false,
      "TopSites": false,
      "SponsoredTopSites": false,
      "Highlights": false,
      "Pocket": false,
      "Stories": false,
      "SponsoredPocket": false,
      "SponsoredStories": false
    },
    "FirefoxSuggest": {
      "WebSuggestions": false,
      "SponsoredSuggestions": false,
      "ImproveSuggest": false,
      "OnlineEnabled": false
    },
    "UserMessaging": {
      "WhatsNew": false,
      "ExtensionRecommendations": false,
      "FeatureRecommendations": false,
      "UrlbarInterventions": false,
      "SkipOnboarding": true,
      "MoreFromMozilla": false,
      "FirefoxLabs": false
    }
  }
}
EOF

# Defaults librewolf.cfg leaves alone. Spelling reads the hunspell-en
# dictionaries in /usr/share/hunspell; the file picker goes through the
# FileChooser portal, which on this desktop is kdos-pick.
install -d "$PKG$_app/browser/defaults/preferences"
cat > "$PKG$_app/browser/defaults/preferences/kdos.js" <<'EOF'
pref("spellchecker.dictionary_path", "/usr/share/hunspell");
pref("widget.use-xdg-desktop-portal.file-picker", 1);
EOF

# The panel reads PNG from hicolor apps/ and never an SVG; the branding
# carries the PNGs at every size it draws.
for _png in browser/branding/librewolf/default*.png; do
	_s=${_png##*/default}
	_s=${_s%.png}
	install -Dm644 "$_png" "$PKG/usr/share/icons/hicolor/${_s}x${_s}/apps/librewolf.png"
done

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/librewolf.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=LibreWolf
GenericName=Web Browser
Comment=Browse the World Wide Web with privacy protections on
Exec=librewolf %u
Icon=librewolf
Terminal=false
StartupNotify=true
StartupWMClass=librewolf
Categories=Network;WebBrowser;
Keywords=web;browser;internet;www;http;librewolf;
MimeType=text/html;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;
Actions=new-window;new-private-window;

[Desktop Action new-window]
Name=New Window
Exec=librewolf --new-window %u

[Desktop Action new-private-window]
Name=New Private Window
Exec=librewolf --private-window %u
EOF
chmod 644 "$PKG/usr/share/applications/librewolf.desktop"
