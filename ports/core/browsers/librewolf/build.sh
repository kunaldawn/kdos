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

patch -p1 -i "$PORT_SRC/fix-rust-target.patch"
patch -p1 -i "$PORT_SRC/musl-pthread-t.patch"
patch -p1 -i "$PORT_SRC/glean-stub.patch"
patch -p1 -i "$PORT_SRC/lfs64.patch"
patch -p1 -i "$PORT_SRC/musl-no-linux-prctl.patch"
patch -p1 -i "$PORT_SRC/rust-lto-thin.patch"
patch -p1 -i "$PORT_SRC/sandbox-sched_setscheduler.patch"
patch -p1 -i "$PORT_SRC/parakeet-cstdint.patch"

_gcc_triple="--gcc-triple=$(gcc -dumpmachine)"
export CC=clang
export CXX=clang++
export CFLAGS="-ftrivial-auto-var-init=zero -fwrapv ${CFLAGS/-std=gnu11/} $_gcc_triple"
export CXXFLAGS="-ftrivial-auto-var-init=zero -fwrapv $CXXFLAGS $_gcc_triple"
export LDFLAGS="$LDFLAGS $_gcc_triple"

export LDFLAGS="$LDFLAGS -Wl,-rpath,/usr/lib/librewolf"

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

export MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE=none
export MOZBUILD_STATE_PATH="$SRC/.mozbuild"
export MOZ_NOSPAM=1
export MOZ_BUILD_DATE="$(date -u -d "@$SOURCE_DATE_EPOCH" +%Y%m%d%H%M%S)"

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
    "ContentAnalysis": {
      "Enabled": false,
      "InterceptionPoints": {
        "Clipboard": { "Enabled": false },
        "ClipboardCopy": { "Enabled": false },
        "Download": { "Enabled": false },
        "DragAndDrop": { "Enabled": false },
        "FileUpload": { "Enabled": false },
        "Print": { "Enabled": false }
      }
    },
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

install -d "$PKG$_app/browser/defaults/preferences"
cat > "$PKG$_app/browser/defaults/preferences/kdos.js" <<'EOF'
pref("spellchecker.dictionary_path", "/usr/share/hunspell");
pref("widget.use-xdg-desktop-portal.file-picker", 1);
EOF

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
