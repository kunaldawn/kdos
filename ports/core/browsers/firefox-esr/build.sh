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

# FIREFOX ESR ON GTK 3, WITH BOTH GDK BACKENDS. cairo-gtk3-x11-wayland makes
# the Wayland and the X11 halves both required, so a missing X client library
# stops configure instead of leaving a browser that cannot start under
# Xwayland. GDK tries Wayland first, which is what runs on kdos-comp.
#
# The application is named firefox-esr: the binary, /usr/lib/firefox-esr, the
# desktop entry and the Wayland app_id (MOZ_APP_REMOTINGNAME) all carry that
# one name, and StartupWMClass has to equal it or the panel groups the window
# under no entry. It also keeps clear of `firefox`, the name kdos-appbox gives
# the boxed browser's shim.
#
# Five of Alpine's patches, verbatim. lfs64: musl has no stat64 or
# lstat64, and a configure that finds the declarations still links nothing.
# musl-no-linux-prctl: <linux/prctl.h> and musl's <sys/prctl.h> both define
# struct prctl_mm_map, and libwebrtc includes both. sandbox-sched_setscheduler:
# musl's pthread_setschedparam is sched_setscheduler, which the media
# sandboxes otherwise kill the process for. fix-rust-target: configure
# matches the C triple against rustc's target list, where a musl x86_64
# triple fits both x86_64-unknown-linux-musl and x86_64-unikraft-linux-musl
# and a vendor other than `unknown` picks neither, so configure stops; the
# patch takes the target from RUST_TARGET.
# glean-stub: the metric accessors only tests call are stubbed out; compiled
# into gkrust they can exhaust rustc's LLVM memory.
patch -p1 -i "$PORT_SRC/fix-rust-target.patch"
patch -p1 -i "$PORT_SRC/glean-stub.patch"
patch -p1 -i "$PORT_SRC/lfs64.patch"
patch -p1 -i "$PORT_SRC/musl-no-linux-prctl.patch"
patch -p1 -i "$PORT_SRC/sandbox-sched_setscheduler.patch"

# Clang and lld, the toolchain Mozilla builds and tests Firefox with; bindgen
# needs libclang whichever compiler builds the C++. The phase's
# -std=gnu11 is dropped: mozbuild sets the language standard of every C file
# itself.
export CC=clang
export CXX=clang++
export CFLAGS="${CFLAGS/-std=gnu11/}"

# rustc's musl targets default to crt-static; gkrust is linked into the
# shared libxul, which takes the shared C library like every other port.
export RUSTFLAGS="-C target-feature=-crt-static"
export RUST_TARGET=x86_64-unknown-linux-musl

# mach creates a virtualenv and would pip-install zstandard and psutil into it
# from the network; `none` builds with the pure-Python copies in the tarball.
export MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE=none
export MOZBUILD_STATE_PATH="$SRC/.mozbuild"
export MOZ_NOSPAM=1
export MOZ_BUILD_DATE="$(date -u -d "@$SOURCE_DATE_EPOCH" +%Y%m%d%H%M%S)"

# No MOZILLA_OFFICIAL: it turns telemetry reporting on by default, and the
# official branding needs only the flag below. MOZ_TELEMETRY_REPORTING is off
# at build time here and DisableTelemetry holds it off at run time.
#
# --without-wasm-sandboxed-libraries: RLBox needs a wasm32-wasi sysroot and a
# rustc with the WebAssembly target, and this tree's rustc is built for X86
# alone; graphite, hunspell, ogg, expat and woff2 then run as native code.
# --disable-eme: the only module is Widevine, a glibc binary fetched from
# Google at run time. --disable-jemalloc: mozjemalloc is unproven on musl, so
# the browser runs on musl's allocator, which is slower under its load.
# libpng is not taken from the system because Firefox needs its APNG patch.
# av1 stays in-tree: --with-system-av1 needs aom as well as dav1d.
# --disable-necko-wifi: the Wi-Fi scanner feeds only network geolocation,
# which asks an online location service.
cat > mozconfig <<EOF
ac_add_options --prefix=/usr
ac_add_options --with-app-name=firefox-esr
ac_add_options MOZ_APP_REMOTINGNAME=firefox-esr
ac_add_options --enable-official-branding
ac_add_options --enable-update-channel=esr
ac_add_options --with-distribution-id=org.kdos
ac_add_options MOZ_TELEMETRY_REPORTING=

ac_add_options --enable-application=browser
ac_add_options --enable-default-toolkit=cairo-gtk3-x11-wayland
ac_add_options --enable-release
ac_add_options --enable-optimize=-O2
ac_add_options --enable-hardening
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
EOF
export MOZCONFIG="$SRC/mozconfig"

./mach build
DESTDIR="$PKG" ./mach install

_app=/usr/lib/firefox-esr

# firefox-esr-bin is a second copy of the same executable, kept for scripts
# that name it.
if [ -f "$PKG$_app/firefox-esr-bin" ] && [ ! -L "$PKG$_app/firefox-esr-bin" ]; then
	ln -sf firefox-esr "$PKG$_app/firefox-esr-bin"
fi

# The lockdown. policies.json is read from the installation's distribution/
# directory and wins over every profile: no telemetry, studies, remote
# improvements or Remote Settings, no application or system add-on update, no
# DNS over HTTPS, no captive-portal or connectivity probes, no Mozilla
# account (Sync), no sponsored or online suggestions, no onboarding, and no
# AI or translation features, which fetch their models from Remote Settings.
# Disabling Remote Settings also stops blocklist and CRLite revocation
# updates; the copies packaged with this release are what is enforced.
install -d "$PKG$_app/distribution"
cat > "$PKG$_app/distribution/policies.json" <<'EOF'
{
  "policies": {
    "DisableTelemetry": true,
    "DisableFirefoxStudies": true,
    "DisableRemoteImprovements": true,
    "DisableRemoteSettingsAndAcceptSecurityConsequences": true,
    "DisableAppUpdate": true,
    "DisableSystemAddonUpdate": true,
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
    "OverrideFirstRunPage": "",
    "OverridePostUpdatePage": "",
    "GenerativeAI": { "Enabled": false, "Locked": true },
    "AIControls": { "Default": { "Value": "blocked", "Locked": true } },
    "FirefoxHome": {
      "Weather": false,
      "SponsoredTopSites": false,
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

cat > "$PKG$_app/distribution/distribution.ini" <<'EOF'
[Global]
id=kdos
version=1.0
about=Firefox ESR for KDOS

[Preferences]
app.distributor="kdos"
app.distributor.channel="firefox-esr"
EOF

# Defaults the policies have no key for. Spelling reads the hunspell-en
# dictionaries in /usr/share/hunspell. The file picker goes through the
# FileChooser portal, which on this desktop is kdos-pick. No media plugin is
# downloaded: OpenH264 and Widevine both come from the network.
install -d "$PKG$_app/browser/defaults/preferences"
cat > "$PKG$_app/browser/defaults/preferences/kdos.js" <<'EOF'
pref("spellchecker.dictionary_path", "/usr/share/hunspell");
pref("widget.use-xdg-desktop-portal.file-picker", 1);
pref("media.gmp-manager.updateEnabled", false);
pref("media.gmp-gmpopenh264.enabled", false);
pref("media.gmp-gmpopenh264.visible", false);
pref("browser.newtabpage.activity-stream.telemetry", false);
pref("datareporting.healthreport.uploadEnabled", false);
pref("datareporting.usage.uploadEnabled", false);
EOF

# The panel reads PNG from hicolor apps/ and never an SVG; the branding
# carries the PNGs at every size it draws.
for _png in browser/branding/official/default*.png; do
	_s=${_png##*/default}
	_s=${_s%.png}
	install -Dm644 "$_png" "$PKG/usr/share/icons/hicolor/${_s}x${_s}/apps/firefox-esr.png"
done

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/firefox-esr.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Firefox
GenericName=Web Browser
Comment=Browse the World Wide Web
Exec=firefox-esr %u
Icon=firefox-esr
Terminal=false
StartupNotify=true
StartupWMClass=firefox-esr
Categories=Network;WebBrowser;
Keywords=web;browser;internet;www;http;firefox;
MimeType=text/html;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;
Actions=new-window;new-private-window;

[Desktop Action new-window]
Name=New Window
Exec=firefox-esr --new-window %u

[Desktop Action new-private-window]
Name=New Private Window
Exec=firefox-esr --private-window %u
EOF
chmod 644 "$PKG/usr/share/applications/firefox-esr.desktop"
