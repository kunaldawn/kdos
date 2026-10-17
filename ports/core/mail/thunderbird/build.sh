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
patch -p1 -i "$PORT_SRC/glean-stub.patch"
patch -p1 -i "$PORT_SRC/lfs64.patch"
patch -p1 -i "$PORT_SRC/musl-no-linux-prctl.patch"
patch -p1 -i "$PORT_SRC/rust-lto-thin.patch"
patch -p1 -i "$PORT_SRC/sandbox-sched_setscheduler.patch"
patch -p1 -i "$PORT_SRC/fix-libresolv-path.patch"
patch -p1 -i "$PORT_SRC/metainfo.patch"

_gcc_triple="--gcc-triple=$(gcc -dumpmachine)"
export CC=clang
export CXX=clang++
export CFLAGS="${CFLAGS/-std=gnu11/} $_gcc_triple"
export CXXFLAGS="$CXXFLAGS $_gcc_triple"
export LDFLAGS="$LDFLAGS $_gcc_triple"

export LDFLAGS="$LDFLAGS -Wl,-rpath,/usr/lib/thunderbird"
export AR=llvm-ar
export NM=llvm-nm
export RANLIB=llvm-ranlib
export RUST_TARGET=x86_64-unknown-linux-musl
unset MOZILLA_OFFICIAL MOZ_TELEMETRY_REPORTING

export RUSTFLAGS="-C target-feature=-crt-static"
cat > "$SRC_ROOT/rustc-dynamic" <<'EOF'
#!/bin/sh
rustc=$1
shift
exec "$rustc" "$@" -C target-feature=-crt-static
EOF
chmod 755 "$SRC_ROOT/rustc-dynamic"
export RUSTC_WRAPPER="$SRC_ROOT/rustc-dynamic"

export MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE=none
export MOZBUILD_STATE_PATH="$SRC_ROOT/mozbuild"
export MOZ_NOSPAM=1
export SHELL=/bin/sh

MOZ_BUILD_DATE=$(date -u -d "@$SOURCE_DATE_EPOCH" +%Y%m%d%H%M%S)
export MOZ_BUILD_DATE

cat > mozconfig <<CONF
ac_add_options --enable-application=comm/mail
ac_add_options --prefix=/usr
ac_add_options --enable-default-toolkit=cairo-gtk3-x11-wayland
ac_add_options --enable-official-branding
ac_add_options --enable-update-channel=release
ac_add_options --with-distribution-id=org.kdos
ac_add_options --with-unsigned-addon-scopes=app,system

ac_add_options --disable-bootstrap
ac_add_options --disable-crashreporter
ac_add_options --disable-updater
ac_add_options --disable-tests
ac_add_options --disable-debug
ac_add_options --disable-debug-symbols
ac_add_options --disable-strip
ac_add_options --disable-install-strip
ac_add_options --disable-jemalloc
ac_add_options --disable-necko-wifi
ac_add_options --disable-cargo-incremental
ac_add_options --without-wasm-sandboxed-libraries

ac_add_options --enable-release
ac_add_options --enable-optimize
ac_add_options --enable-hardening
ac_add_options --enable-linker=lld
ac_add_options --with-libclang-path=/usr/lib
ac_add_options --enable-dbus
ac_add_options --enable-alsa
ac_add_options --enable-pulseaudio
ac_add_options --disable-jack
ac_add_options --disable-sndio

ac_add_options --with-system-nspr
ac_add_options --with-system-nss
ac_add_options --with-system-icu
ac_add_options --with-system-jpeg
ac_add_options --with-system-webp
ac_add_options --with-system-libvpx
ac_add_options --with-system-libevent
ac_add_options --with-system-ffi
ac_add_options --with-system-zlib
ac_add_options --with-system-pixman
ac_add_options --with-system-jsonc
ac_add_options --with-system-bz2
ac_add_options --with-system-pipewire
ac_add_options --with-system-gbm
ac_add_options --with-system-libdrm

mk_add_options MOZ_OBJDIR=$SRC/obj
CONF
export MOZCONFIG="$SRC/mozconfig"

_jobs=${MAKEFLAGS#-j}
unset MAKEFLAGS
./mach build ${_jobs:+-j"$_jobs"}
DESTDIR="$PKG" ./mach install

_app=/usr/lib/thunderbird

install -Dm644 /dev/stdin "$PKG$_app/defaults/pref/kdos-prefs.js" <<'PREFS'
pref("intl.locale.requested", "");
pref("spellchecker.dictionary_path", "/usr/share/hunspell");
pref("mail.shell.checkDefaultMail", false);
pref("mailnews.start_page.enabled", false);
pref("app.update.auto", false);
pref("extensions.update.enabled", false);
pref("extensions.update.autoUpdateDefault", false);
pref("extensions.getAddons.cache.enabled", false);
pref("datareporting.healthreport.uploadEnabled", false);
pref("datareporting.policy.dataSubmissionEnabled", false);
pref("toolkit.telemetry.enabled", false);
pref("toolkit.telemetry.unified", false);
pref("toolkit.telemetry.archive.enabled", false);
pref("network.captive-portal-service.enabled", false);
pref("network.connectivity-service.enabled", false);
pref("browser.safebrowsing.malware.enabled", false);
pref("browser.safebrowsing.phishing.enabled", false);
pref("browser.safebrowsing.downloads.remote.enabled", false);
PREFS

install -Dm644 /dev/stdin "$PKG$_app/distribution/policies.json" <<'POLICIES'
{
  "policies": {
    "DisableAppUpdate": true,
    "DisableTelemetry": true,
    "ExtensionUpdate": false
  }
}
POLICIES

ln -sfn /usr/share/hunspell "$PKG$_app/dictionaries"
ln -sfn /usr/share/hyphen "$PKG$_app/hyphenation"

_brand=comm/mail/branding/thunderbird
for s in 16 22 24 32 48 64 128 256; do
	install -Dm644 "$_brand/default$s.png" \
		"$PKG/usr/share/icons/hicolor/${s}x$s/apps/thunderbird.png"
done
install -Dm644 "$_brand/net.thunderbird.Thunderbird.appdata.xml" \
	"$PKG/usr/share/metainfo/net.thunderbird.Thunderbird.appdata.xml"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/thunderbird.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Thunderbird
GenericName=Mail, Calendar and Contacts
Comment=Read and write mail, keep a calendar and address book, follow feeds
Exec=thunderbird %u
Icon=thunderbird
Terminal=false
StartupWMClass=thunderbird
Categories=Network;Email;Calendar;ContactManagement;Feed;
MimeType=message/rfc822;x-scheme-handler/mailto;text/calendar;text/vcard;text/x-vcard;
Keywords=mail;email;e-mail;calendar;contacts;address book;feeds;rss;news;imap;smtp;
DESKTOP
chmod 644 "$PKG/usr/share/applications/thunderbird.desktop"
